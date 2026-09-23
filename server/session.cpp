#include "session.h"

#include "worker.h"
#include "../memoryPool/memoryPool.h"

#include <netinet/in.h>
#include <linux/netfilter_ipv4.h>
#include <arpa/inet.h>


session::session(std::unique_ptr<worker> &worker,tcp::socket &&wifiClientSocket ):
worker_(worker),wifiClientSocket_(std::move(wifiClientSocket)),currentHeapNumber_(0),
trafficBusy_(false),currentCmdStatus_(cmdStatus::newSession){
    kcp_=ikcp_create(1,this);//我们不用kcp自带的令牌,我们自己创建內令机制
    kcp_->output = kcpCallBack;
    ikcp_setmtu(kcp_, 1250);
    kcp_->rx_minrto = 10;
    ikcp_nodelay(kcp_, 1, 10, 2, 1);
    wifiClientSocketBuffer_=worker_->getCPtrFunc(bufferSize);
    //remoteServerSocketBuffer_=worker_->getCPtrFunc(bufferSize);

}

session::~session() {
    ikcp_release(kcp_);
    worker_->freeMemory(wifiClientSocketBuffer_);
    worker_->freeMemory(this);
    //worker_->freeMemory(remoteServerSocketBuffer_);
}

void session::start() {
    uint32_t now=worker::getClockMs();
    LogD("调试信息");
    ikcp_update(kcp_,now);
    uint32_t nextTime=ikcp_check(kcp_,now);//算出真正下一次需要驱动的时间,而不是直接用now入堆
    worker_->getTimeWheel().mountTask(this,nextTime);
    //TODO 是否使用weak_ptr
    sockaddr_in orig_dst{};
    socklen_t addrlen = sizeof(orig_dst);
    int fd = wifiClientSocket_.native_handle();

    if (getsockopt(fd, SOL_IP, SO_ORIGINAL_DST, &orig_dst, &addrlen) == 0) {
        // 将 sockaddr_in 转换为 asio::ip::tcp::endpoint
        asio::ip::address_v4::bytes_type ip_bytes;
        std::memcpy(ip_bytes.data(), &orig_dst.sin_addr.s_addr, 4);

        asio::ip::address_v4 addr(ip_bytes);
        int port = ntohs(orig_dst.sin_port);

        endpoint_ = tcp::endpoint(addr, port);

        LogD("成功获取真实目标: " + endpoint_.address().to_string() + ":" + std::to_string(port));
    } else {
        // 如果失败（比如直接连10950端口，没有经过NAT），退回到本地获取
        perror("getsockopt SO_ORIGINAL_DST failed");
        endpoint_ = wifiClientSocket_.local_endpoint();
    }
    sendIpAndportToServer();
}

void session::sendIpAndportToServer() {
    statusAndData statusAndData_;
    statusAndData_.status_=static_cast<cmdStatus>(htonl(static_cast<int>(cmdStatus::newSession)));
    statusAndData_.ipAndPort_.ip_ = htonl(endpoint_.address().to_v4().to_uint());
    statusAndData_.ipAndPort_.port_ = htons(endpoint_.port());
    ikcp_send(kcp_, (const char*)&statusAndData_, statusAndDataSize);
    ikcp_flush(kcp_);
    currentCmdStatus_=cmdStatus::normal;
    receiveTcpFromWifiCilentMessage();
}

void session::closeSession() {

    worker_->getTimeWheel().removeTask(this);
}

void session::doCloseSession(void *current) {
    //时间片跨度不够 直接采用worker上的定时器进行分离
    //30s以后从map中分离
}

uint32_t session::driveSessionTimeClock(void *current) {
    //这个回调函数来驱动kcp
    uint32_t now=((session*)current)->worker_->getClockMs();
    ikcp_update(((session*)current)->kcp_,now);
    uint32_t nextTime = ikcp_check(((session*)current)->kcp_, now);
    return nextTime - now;
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
    LogD("调试信息-->{}",front.second);
    asio::async_write(wifiClientSocket_,
        asio::buffer(((statusAndData*)front.first->data())->assistPoint,front.second-cmdStatusSize),
        [this](boost::system::error_code ec,size_t byteHadSend) {
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
    wifiClientSocket_.async_read_some(
        asio::buffer((char*)wifiClientSocketBuffer_+cmdStatusSize,bufferSize-cmdStatusSize),
        [this](boost::system::error_code ec,size_t byteHadRead) {
            LogD("调试信息-->{}",byteHadRead);
            if (ec) {
                LogE("出现问题");
                return;
            }
            if (byteHadRead==0) {
                return; // 空数据不处理
            }
            statusAndData*point=(statusAndData*)wifiClientSocketBuffer_;
            point->status_=static_cast<cmdStatus>(htonl(static_cast<int>(cmdStatus::normal)));
            int ret = ikcp_send(kcp_,(char *)point, byteHadRead+cmdStatusSize);
            ikcp_flush(kcp_);//立即刷新调用kcpCallBack
            //准备进行流量控制查看是否回调receiveTcpFromWifiCilentMessage
            checkTrafficStatus();
            //由于tcp层自带背压机制,所以这里不直接进行回调
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
        statusAndData*point=(statusAndData*)dataPtr->data();
        switch (static_cast<int>(ntohl(static_cast<uint32_t>(point->status_)))) {
            case static_cast<int>(cmdStatus::normal):
                trySendTcpToWifiClientMessage(std::make_pair(dataPtr,len));
                break;
            case static_cast<int>(cmdStatus::removeSession):

                break;
            default:
                break;
        }
    }
}

int session::kcpCallBack(const char *buf, int len, ikcpcb *kcp, void *user) {
    //len<1250;
    session* currentSession=(session*)user;
    //內令设置
    LogD("调试信息-->{}",len);
    std::shared_ptr<std::array<uint8_t, bufferSize>> bufferPtr=
    currentSession->worker_->getSharedPtrFunc(len+bufferPaddingSize);
    char* const  assistPoint=(char*)bufferPtr->data();
    worker::SessionId* headerPtr=(worker::SessionId*)(assistPoint+keyAndIvOffSet);
    *headerPtr=currentSession->sessionId_;
    memcpy(assistPoint+keyAndIvOffSet+worker::sessionIdSize,buf,len);
    len=currentSession->worker_->
    getFastAesGcm().doEncrypt(assistPoint,len+worker::sessionIdSize);
    currentSession->worker_->tryTosendUdpMessageToRemoteServer(
        std::make_pair(bufferPtr,len));
    return 0;
}


