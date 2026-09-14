#include "worker.h"

#include "session.h"


worker::worker(int remoteServerPort):remoteServerPort_(remoteServerPort),
sessionSize(0),ioCtx_(1),workGuard_(asio::make_work_guard(ioCtx_)),
remoteServerSocker_(ioCtx_.get_executor()) {}

worker::~worker() {
    //TODO 内存池销毁
}




void worker::start() {
    threadPtr_=std::make_shared<std::thread>([this]() {
        asio::ip::address ipAddr=asio::ip::make_address(ForeignServerIpaddr);
         remoteServerSocker_.async_connect(udp::endpoint(ipAddr, remoteServerPort_),
             [](boost::system::error_code ec) {
             if (ec) {
                 LogE("出现问题");
                 return;
             }

         });
        ioCtx_.run();
    });
}

void worker::registerSession(std::shared_ptr<session> &sessionPtr) {
    //TODO存在拷贝疑问
    ++sessionSize;
    sessionPtr->localSessionId_=sessionSize;
    sessionList_.push_back(sessionPtr);
    sessionPtr->start();
}

void worker::sendUdpMessageToRemoteServer() {


}

void worker::receiveUdpMessageFromRemoteServer() {


}

void worker::freeMemory(void* dataPtr) {
    memoryPool_.freeMemory(dataPtr);
}

std::array<uint8_t, 2048>* worker::getMemory(int memorySize) {
    return (std::array<uint8_t, 2048> *)memoryPool_.mallocMemory(memorySize);
}


