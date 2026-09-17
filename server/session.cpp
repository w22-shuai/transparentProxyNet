#include "session.h"

#include "worker.h"
#include "../memoryPool/memoryPool.h"




session::session(std::unique_ptr<worker> &worker,tcp::socket &&wifiClientSocket ):
worker_(worker),wifiClientSocket_(std::move(wifiClientSocket)),currentHeapNumber_(0),
trafficBusy_(false){
    kcp_=ikcp_create(1,this);//我们不用kcp自带的令牌,我们自己创建內令机制
    kcp_->output = kcpCallBack;
    ikcp_setmtu(kcp_, 1250);
    kcp_->rx_minrto = 10;
    ikcp_nodelay(kcp_, 1, 10, 2, 1);

    wifiClientSocketBuffer_=worker_->getCPtrFunc(bufferSize);
    //remoteServerSocketBuffer_=worker_->getCPtrFunc(bufferSize);
    currentCmdStatus_=cmdStatus::newSession;
}

session::~session() {
    ikcp_release(kcp_);
    worker_->freeMemory(wifiClientSocketBuffer_);
    //worker_->freeMemory(remoteServerSocketBuffer_);
}

void session::start() {
    uint32_t now=worker::getClockMs();
    ikcp_update(kcp_,now);
    worker_->pushNewSessionToHeap(now,shared_from_this());
    //TODO 是否使用weak_ptr
    receiveTcpFromWifiCilentMessage();
}

void session::closeSession() {


}

void session::timeToWork(uint32_t now) {
    ikcp_update(kcp_,now);
    //worker通过调用这个回调函数来驱动kcp
}

void session::updateTimeToWorkerHeap(uint32_t now) {
    //只要 KCP 的内部状态发生了变化（Update、Input、Send），就调用 ikcp_check
    uint32_t nextTime = ikcp_check(kcp_, now);
    worker_->updateSessionTimerInHeap(currentHeapNumber_, nextTime);
}

void session::trySendTcpToWifiClientMessage(std::pair<std::shared_ptr<std::array<uint8_t,bufferSize>>,int>&& pair) {
    bool isEmpty=tcpDataWaitForSendDeque_.empty();
    tcpDataWaitForSendDeque_.push_back(std::move(pair));
    if (isEmpty) {
        sendTcpToWifiClientMessage();
    }
}

void session::sendTcpToWifiClientMessage() {
    std::pair<std::shared_ptr<std::array<uint8_t,bufferSize>>,int> &front=
      tcpDataWaitForSendDeque_.front();
    std::shared_ptr<session> self=shared_from_this();
    asio::async_write(wifiClientSocket_,
        asio::buffer(front.first->data(),front.second),
        [this,self](boost::system::error_code ec,size_t byteHadSend) {
            tcpDataWaitForSendDeque_.pop_front();
            if (ec) {
                LogE("回写WiFi客户端失败");
                closeSession();
                return;
            }
            if (!tcpDataWaitForSendDeque_.empty()) {
                sendTcpToWifiClientMessage();
            }
    });

}

void session::receiveTcpFromWifiCilentMessage() {
    std::shared_ptr<session> self=shared_from_this();
    wifiClientSocket_.async_read_some(
        asio::buffer(wifiClientSocketBuffer_,bufferSize),
        [this,self](boost::system::error_code ec,size_t byteHadRead) {
            if (ec) {
                LogE("出现问题");
                return;
            }
            if (byteHadRead==0) {
                return; // 空数据不处理
            }
            int ret = ikcp_send(kcp_, (const char*)wifiClientSocketBuffer_, byteHadRead);
            //TODO 返回值疑问
            //准备进行流量控制查看是否回调receiveTcpFromWifiCilentMessage
            checkTrafficStatus();
            //receiveTcpFromWifiCilentMessage(); 由于tcp层自带背压机制,所以这里不直接进行回调
    });
}

void session::checkTrafficStatus() {
    if (ikcp_waitsnd(kcp_) >= kcpSendWindowHighWaterMark) {
        trafficBusy_ = true;
        LogD("KCP发送拥堵");
    } else {
        receiveTcpFromWifiCilentMessage();
    }
}

void session::inputToKcp(void*dataPtr,int len) {
    ikcp_input(kcp_,(const char *)dataPtr,len);
    tryToReadFromKcp();
    //此处应当可以配合receiveTcpFromWifiCilentMessage函数了
    //采用事件驱动进行流控
    if (trafficBusy_ && ikcp_waitsnd(kcp_) < kcpSendWindowHighWaterMark*2/3) {
        //选择2/3避免抖动
        trafficBusy_ = false;
        LogD("KCP拥堵缓解");
        receiveTcpFromWifiCilentMessage();
    }
}

void session::tryToReadFromKcp() {
    for (;;) {
        int size=ikcp_peeksize(kcp_);
        if (size<=0) {
            break; //没有更多已经组装完整的数据了
        }
        std::shared_ptr<std::array<uint8_t,bufferSize>> dataPtr=
            worker_->getSharedPtrFunc(size);
        int len=ikcp_recv(kcp_,(char*)dataPtr->data(),size);
        if (len<0) {
            break;
        }
        trySendTcpToWifiClientMessage(std::make_pair(dataPtr,len));
    }

}

int session::kcpCallBack(const char *buf, int len, ikcpcb *kcp, void *user) {
    //len<1250;
    session* currentSession=(session*)user;
    //內令设置
    std::shared_ptr<std::array<uint8_t, bufferSize>> bufferPtr=
    currentSession->worker_->getSharedPtrFunc(len+bufferPaddingSize);
    char* const  assistPoint=(char*)bufferPtr.get();
    cmdHeader* headerPtr=(cmdHeader*)assistPoint;
    headerPtr->sessionId_=currentSession->sessionId_;
    memcpy(assistPoint+cmdHeaderSize,buf,len);
    //內令+真实数据
    switch (static_cast<int>(currentSession->currentCmdStatus_)) {
        case static_cast<int>(cmdStatus::normal):
            headerPtr->cmd_=cmdStatus::normal;
            break;
        case static_cast<int>(cmdStatus::newSession):
            headerPtr->cmd_=cmdStatus::newSession;
            break;
        case static_cast<int>(cmdStatus::removeSession):
            headerPtr->cmd_=cmdStatus::removeSession;
            break;
        default:
            break;
    }
    len=currentSession->worker_->
    getFastAesGcm().doEncrypt(assistPoint,len+cmdHeaderSize);
    currentSession->worker_->tryTosendUdpMessageToRemoteServer(
        std::make_pair(bufferPtr,len));
    return 0;
}


