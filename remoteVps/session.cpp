#include "session.h"

#include "worker.h"



session::session(worker *worker,udp::endpoint& endpoint,tcp::socket&&targetServerTcpSocket):worker_(worker),
homeClienEndpoint_(std::move(endpoint)),targetServerTcpSocket_(std::move(targetServerTcpSocket)),
currentCmdStatus_(cmdStatus::newSession),enableToSendToTargetServer_(false),close_(false),trafficBusy_(false){
kcp_=ikcp_create(1,this);//我们不用kcp自带的令牌,我们自己创建內令机制
    kcp_->output = kcpCallBack;
    ikcp_setmtu(kcp_, 1250);
    kcp_->rx_minrto = 10;
    ikcp_nodelay(kcp_, 1, 10, 2, 1);
    targetServerTcpSocketBuffer_=worker_->getCPtrFunc(bufferSize);
    targetServerTcpSocket_.open(tcp::v4());  // 必须先 open,才有合法 fd

    int mark = 1;  // 任意非0值，要和 iptables 规则里的值对上
    if (setsockopt(targetServerTcpSocket_.native_handle(), SOL_SOCKET, SO_MARK,
                   &mark, sizeof(mark)) != 0) {
        LogE("掩码设置失败");
        }
}


session::~session() {
    ikcp_release(kcp_);
    worker_->freeMemory(targetServerTcpSocketBuffer_);
    worker_->freeMemory(this);
}

void session::start() {
    uint32_t now=worker::getClockMs();
    ikcp_update(kcp_,now);
    uint32_t nextTime=ikcp_check(kcp_,now);//算出真正下一次需要驱动的时间,而不是直接用now入堆
    worker_->getTimeWheel().mountTask(this,nextTime);
}

void session::closeSession() {
    worker_->getTimeWheel().removeTask(this);
}

void session::doCloseSession(void *current) {
    //时间片跨度不够 直接采用worker上的定时器进行分离
    //30s以后从map中分离
}

uint32_t session::driveSessionTimeClock(void *current) {
    uint32_t now=((session*)current)->worker_->getClockMs();
    ikcp_update(((session*)current)->kcp_,now);
    uint32_t nextTime = ikcp_check(((session*)current)->kcp_, now);
    return nextTime - now;
}

void session::trySendTcpToTargetServerMessage(std::pair<std::shared_ptr<std::array<uint8_t, bufferSize>>,int> &&pair)
{
    if (!enableToSendToTargetServer_) {
        tcpDataWaitForSendDeque_.push_back(std::move(pair));
        return;
    }
    bool isEmpty=tcpDataWaitForSendDeque_.empty();
    tcpDataWaitForSendDeque_.push_back(std::move(pair));
    if (isEmpty) {
        sendTcpToTargetServerMessage();
    }
}

void session::sendTcpToTargetServerMessage() {
    std::pair<std::shared_ptr<std::array<uint8_t,bufferSize>>,int> &front=
      tcpDataWaitForSendDeque_.front();
    LogD("发送数据-->{}",front.second-cmdStatusSize);
    asio::async_write(targetServerTcpSocket_,
        asio::buffer(((statusAndData*)front.first->data())->assistPoint,front.second-cmdStatusSize),
        [this](boost::system::error_code ec,size_t byteHadSend) {
            tcpDataWaitForSendDeque_.pop_front();
            if (ec) {
                LogE("发送数据到目标服务器失败-->{}",ec.what());
                closeSession();
                return;
            }
            if (!tcpDataWaitForSendDeque_.empty()) {
                sendTcpToTargetServerMessage();
            }
    });

}

void session::receiveTcpFromTargetServerMessage() {
    //(char*)防止偏移出问题
    targetServerTcpSocket_.async_read_some(
        asio::buffer((uint8_t*)targetServerTcpSocketBuffer_+cmdStatusSize,bufferSize-cmdStatusSize),
        [this](boost::system::error_code ec,size_t byteHadRead) {
            if (ec) {
                LogE("出现问题-->{}",ec.what());
                return;
            }
            if (byteHadRead==0) {
                return; // 空数据不处理
            }
            statusAndData*point=(statusAndData*)targetServerTcpSocketBuffer_;
            point->status_=static_cast<cmdStatus>(htonl(static_cast<int>(cmdStatus::normal)));
            int ret = ikcp_send(kcp_,(char*)point, byteHadRead+cmdStatusSize);
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
        receiveTcpFromTargetServerMessage();
    }
}

void session::inputToKcp(void*dataPtr, int len) {
    ikcp_input(kcp_,(const char *)dataPtr,len);
    tryToReadFromKcp();

    if (trafficBusy_ && ikcp_waitsnd(kcp_) < kcpSendWindowHighWaterMark*2/3) {
        //选择2/3避免抖动
        trafficBusy_ = false;
        LogD("KCP拥堵缓解");
        receiveTcpFromTargetServerMessage();
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
            case static_cast<int>(cmdStatus::newSession):
                if (currentCmdStatus_==cmdStatus::newSession) {
                    //与目标服务器握手
                    //TODO 查询是否正确
                    tcp::endpoint ep(asio::ip::make_address_v4(ntohl(point->ipAndPort_.ip_)),
                        ntohs(point->ipAndPort_.port_));
                    handShakeWithtargetServer(ep);
                }
                break;
            case static_cast<int>(cmdStatus::normal):
                trySendTcpToTargetServerMessage(std::make_pair(dataPtr,len));
                break;
            case static_cast<int>(cmdStatus::removeSession):

                break;
            default:
                break;
        }
    }

}

void session::handShakeWithtargetServer(tcp::endpoint &ep) {
    targetServerTcpSocket_.async_connect(ep,[this](boost::system::error_code ec) {
        if (ec) {
        LogE("目标服务器tcp握手失败");
        return;
      }
      LogD("连接目标服务器成功");
      //此时才能开启盲转发收发功能
      enableToSendToTargetServer_=true;
      currentCmdStatus_=cmdStatus::normal;
      bool isEmpty=tcpDataWaitForSendDeque_.empty();
      receiveTcpFromTargetServerMessage();
      if (!isEmpty) {
          sendTcpToTargetServerMessage();
      }
     });
}

int session::kcpCallBack(const char *buf, int len, ikcpcb *kcp, void *user) {
    session* currentSession=(session*)user;
    //內令设置
    std::shared_ptr<std::array<uint8_t, bufferSize>> bufferPtr=
    currentSession->worker_->getSharedPtrFunc(len+bufferPaddingSize);
    char* const  assistPoint=(char*)bufferPtr->data();
    worker::SessionId* headerPtr=(worker::SessionId*)(assistPoint+worker::keyAndIvOffSet);
    *headerPtr=currentSession->sessionId_;
    memcpy(assistPoint+worker::keyAndIvOffSet+worker::sessionIdSize,buf,len);
    len=currentSession->worker_->
    getFastAesGcm().doEncrypt(assistPoint,len+worker::sessionIdSize);
    currentSession->worker_->tryTosendUdpMessageToRemoteServer(
        std::make_pair(currentSession->homeClienEndpoint_,std::make_pair(bufferPtr,len)));
        //切记不能使用引用
    return 0;
}