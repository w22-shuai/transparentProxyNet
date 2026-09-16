#include <sys/random.h>
#include <array>
#include <stdexcept>


#include "worker.h"
#include "session.h"





worker::worker(int remoteServerPort):remoteServerPort_(remoteServerPort),
sessionSize(0),ioCtx_(1),workGuard_(asio::make_work_guard(ioCtx_)),
remoteServerSocker_(ioCtx_.get_executor()),udpReceiveBuffer(nullptr) {}

worker::~worker() {
    //TODO 内存池销毁
    freeMemory(udpReceiveBuffer);
}



void worker::start() {
    threadPtr_=std::make_shared<std::thread>([this]() {
        //初始化内存
        udpReceiveBuffer=getMemory(2048);//udp mtu2048完全够用
        asio::ip::address ipAddr=asio::ip::make_address(ForeignServerIpaddr);
         remoteServerSocker_.async_connect(udp::endpoint(ipAddr, remoteServerPort_),
             [this](boost::system::error_code ec) {
             if (ec) {
                 LogE("出现问题");
                 return;
             }

         });
         receiveUdpMessageFromRemoteServer();
         ioCtx_.run();
    });
}

std::array<uint8_t, 16>  worker::getSessionId() {
    std::array<uint8_t, 16> id;
    ssize_t n = getrandom(id.data(), id.size(), 0);
    if (n != static_cast<ssize_t>(id.size())) {
        throw std::runtime_error("随机数生成失败"); // 极罕见,内核熵源异常时才会发生
    }
    return id;
}




void worker::sendUdpMessageToRemoteServer(std::pair<std::shared_ptr<std::array<uint8_t,2048>>,int>&&pair) {
    //流控背压机制,所有sesion都走这里所以做一下留空,再者同一socket不允许同时读写
    bool isEmpty=udpSocketWaitForSendDeque_.empty();
    udpSocketWaitForSendDeque_.push_back(pair);
    if (isEmpty) {
        dosendUdpMessageToRemoteServer();
    }
}


void worker::dosendUdpMessageToRemoteServer() {
    std::pair<std::shared_ptr<std::array<uint8_t,2048>>,int> &pair=
        udpSocketWaitForSendDeque_.front();
    remoteServerSocker_.async_send(asio::buffer(pair.first.get(),pair.second),
        [this](boost::system::error_code ec,size_t byteHadSend) {
            udpSocketWaitForSendDeque_.pop_front();
            if (ec) {
            LogE("出现问题");
            return;
            }
            if (!udpSocketWaitForSendDeque_.empty()) {
                dosendUdpMessageToRemoteServer();
            }
    });
}


void worker::registerSession(std::shared_ptr<session> &sessionPtr) {
    ++sessionSize;
    //使用摘要算法制造16字节ID,存入absl哈希表进行匹配
    std::array<uint8_t, 16> randomId=getSessionId();
    sessionPtr->setSessionId(randomId);//设置ID
    //将当前本地id session键值对存入哈希表,由于面向无连接状态,后续收到udp需要查找
    sessionMap_.emplace(randomId,sessionPtr);
    sessionPtr->start();
}


void worker::receiveUdpMessageFromRemoteServer() {
    remoteServerSocker_.async_receive(asio::buffer(udpReceiveBuffer,bufferSize),
    [this](boost::system::error_code ec,size_t byteHadRead) {
        if (ec) {
        LogE("出现问题");
        return;
        }
        int len=fastAes.doDecrypt(udpReceiveBuffer,byteHadRead);
        session::cmdHeader *headerPoint=(session::cmdHeader*)udpReceiveBuffer;
        auto result=sessionMap_.find(headerPoint->sessionId_);
        if (result != sessionMap_.end()) {
            std::shared_ptr<session>sessionPtr=result->second;
            switch (static_cast<int>(headerPoint->cmd_)) {
                case static_cast<int>(session::cmdStatus::normal):
                    sessionPtr->inputToKcp(udpReceiveBuffer,len);
                    break;
                case static_cast<int>(session::cmdStatus::newSession):
                    sessionPtr->currentCmdStatus_=session::cmdStatus::normal;
                    sessionPtr->inputToKcp(udpReceiveBuffer,len);
                    break;
                case static_cast<int>(session::cmdStatus::removeSession):
                    //TODO
                    break;
                default:
                    break;
            }
        }
        receiveUdpMessageFromRemoteServer();
    });
}

void worker::freeMemory(void* dataPtr) {
    memoryPool_.freeMemory(dataPtr);
}

void worker::cleanMemoryBlock() {
    //定时清理内存池
    memoryPool_.freeOldMemoryBlock();
}

std::array<uint8_t, 2048>* worker::getMemory(int memorySize) {
    return (std::array<uint8_t, 2048> *)memoryPool_.mallocMemory(memorySize);
}


