#include "session.h"

#include "worker.h"
#include "../memoryPool/memoryPool.h"




session::session(std::unique_ptr<worker> &worker,tcp::socket &&wifiClientSocket ):
worker_(worker),wifiClientSocket_(std::move(wifiClientSocket)) {
    kcp_=ikcp_create(1,this);//我们不用kcp自带的令牌,我们自己创建內令机制
    kcp_->output = kcpCallBack;
    ikcp_setmtu(kcp_, 1250);
    kcp_->rx_minrto = 10;
    ikcp_nodelay(kcp_, 1, 10, 2, 1);
    ikcp_wndsize(kcp_, 4096, 4096); // 窗口大小
    wifiClientSocketBuffer_=getCPtrFunc(bufferSize);
    remoteServerSocketBuffer_=getCPtrFunc(bufferSize);
    currentCmdStatus_=cmdStatus::newSession;
}

session::~session() {
    ikcp_release(kcp_);
    worker_->freeMemory(wifiClientSocketBuffer_);
    worker_->freeMemory(remoteServerSocketBuffer_);
}

void session::start() {
    receiveTcpFromWifiCilentMessage();
}

void session::sendTcpToWifiClientMessage() {

}

std::array<uint8_t, session::bufferSize>* session::getCPtrFunc(int memorySize) {
    return worker_->getMemory(memorySize);
}

//定制智能指针析构做法,减少代码量
std::shared_ptr<std::array<uint8_t, session::bufferSize>> session::getSharedPtrFunc(int memorySize) {
    return std::shared_ptr<std::array<uint8_t,bufferSize>>
    (worker_->getMemory(memorySize),[this](void* dataPoint) {
        worker_->freeMemory(dataPoint);
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
            std::shared_ptr<std::array<uint8_t,bufferSize>>dataPtr=getSharedPtrFunc(byteHadRead);
            memcpy(dataPtr.get(),wifiClientSocketBuffer_,byteHadRead);
            tryToSendToKcp(std::make_pair(dataPtr,byteHadRead));
            receiveTcpFromWifiCilentMessage();
    });
}

void session::tryToSendToKcp(std::pair<std::shared_ptr<std::array<uint8_t,bufferSize>>,int>&& pair) {
    //是否使用背压机制

}

void session::sendToKcp() {
    //ikcp_send 准备塞入kcp
}


void session::inputToKcp(void*dataPtr,int len) {
    ikcp_input(kcp_,(const char *)dataPtr,len);
    tryToReadFromKcp();
}

void session::tryToReadFromKcp() {


}



int session::kcpCallBack(const char *buf, int len, ikcpcb *kcp, void *user) {
    //len<1250;
    session* currentSession=(session*)user;
    //內令设置
    std::shared_ptr<std::array<uint8_t, bufferSize>> bufferPtr=
        std::shared_ptr<std::array<uint8_t, bufferSize>>
    (currentSession->getSharedPtrFunc(len+bufferPaddingSize));
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
    currentSession->worker_->sendUdpMessageToRemoteServer(
        std::make_pair(bufferPtr,len));
    return 0;
}


