#include "session.h"

#include "worker.h"
#include "../memoryPool/memoryPool.h"




session::session(std::unique_ptr<worker> &worker,tcp::socket &&wifiClientSocket ):
worker_(worker),wifiClientSocket_(std::move(wifiClientSocket)) {
    kcp_=ikcp_create(1,this);//我们不用kcp自带的令牌,我们自己创建內令机制
    kcp_->output = kcpCallBack;
    ikcp_setmtu(kcp_, 1350);
    kcp_->rx_minrto = 10;
    ikcp_nodelay(kcp_, 1, 10, 2, 1);
    ikcp_wndsize(kcp_, 4096, 4096); // 窗口大小

}

session::~session() {
    ikcp_release(kcp_);
}

void session::start() {
    receiveTcpFromWifiCilentMessage();
}

void session::sendTcpToWifiClientMessage() {

}

//定制智能指针析构做法,减少代码量
std::shared_ptr<std::array<uint8_t, 2048>> session::getSharedPtrFunc(int memorySize) {
    return std::shared_ptr<std::array<uint8_t,2048>>(worker_->getMemory(memorySize),[this](void* dataPoint) {
        worker_->freeMemory(dataPoint);
    });
}

void session::receiveTcpFromWifiCilentMessage() {
    std::shared_ptr<session> self=shared_from_this();
    std::shared_ptr<std::array<uint8_t,2048>> dataPtr=getSharedPtrFunc(2048);
    wifiClientSocket_.async_read_some(
        asio::buffer(dataPtr.get(),dataPtr->size()),
        [this,self,dataPtr](boost::system::error_code ec,size_t byteHadRead) {
        if (ec) {
            LogE("出现问题");
            return;
        }
        if (byteHadRead==0) {
            return; // 空数据不处理
        }
        //TODO
        // bool isEmpty=udpDataWaitForSendDeque.empty();
        // udpDataWaitForSendDeque.emplace_back(std::make_pair(dataPtr,byteHadRead));
        // if (isEmpty) {
        //     doSendToKcp();
        // }
        receiveTcpFromWifiCilentMessage();
    });
}

void session::doSendToKcp() {

}

void session::receiveFromKcp() {

}

int session::kcpCallBack(const char *buf, int len, ikcpcb *kcp, void *user) {

    return 0;
}


