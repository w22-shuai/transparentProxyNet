#include <sys/random.h>
#include <array>
#include <stdexcept>


#include "worker.h"

#include "server.h"
#include "session.h"


worker::worker(int remoteServerPort,server&server):remoteServerPort_(remoteServerPort),
sessionSize(0),ioCtx_(1),workGuard_(asio::make_work_guard(ioCtx_)),
remoteServerSocker_(ioCtx_.get_executor()),udpReceiveBuffer(nullptr),
kcpUpdateTimer_(ioCtx_.get_executor()),server_(server) {



}

worker::~worker() {
    //TODO 内存池销毁
    freeMemory(udpReceiveBuffer);
}

void worker::start() {
    threadPtr_=std::make_shared<std::thread>([this]() {
        //初始化内存
        udpReceiveBuffer=getMemory(bufferSize);//udp mtu2048完全够用
        asio::ip::address ipAddr=asio::ip::make_address(ForeignServerIpaddr);
         remoteServerSocker_.async_connect(udp::endpoint(ipAddr, remoteServerPort_),
             [this](boost::system::error_code ec) {
             if (ec) {
                 LogE("出现问题");
                 return;
             }
            //LogD("子线程准备完毕");
            ++server_.sonThreadStatus_;
            timeWheel_.operatorFunction_.ctwCallBack_=session::driveSessionTimeClock;
            timeWheel_.operatorFunction_.rtCallBack_=session::doCloseSession;
            startTimeWheel();//时间片启动
            server_.getConditionVariable().notify_one();
            receiveUdpMessageFromRemoteServer();
         });
         ioCtx_.run();
    });
}

void worker::startTimeWheel() {
    uint32_t now=getClockMs();
    uint32_t delayMs=timeWheel<void>::deltaTime_;
    kcpUpdateTimer_.expires_after(std::chrono::milliseconds(delayMs));
    kcpUpdateTimer_.async_wait([this](const boost::system::error_code &ec) {
        timeWheel_.doCurrentTimeWheel();
        startTimeWheel();
    });
}

std::array<uint8_t, 16> worker::getSessionId() {
    std::array<uint8_t, 16> id;
    ssize_t n = getrandom(id.data(), id.size(), 0);
    if (n != static_cast<ssize_t>(id.size())) {
        throw std::runtime_error("随机数生成失败");
    }
    return id;
}

uint32_t worker::getClockMs() {
    using namespace std::chrono;
    static const steady_clock::time_point start=steady_clock::now();
    return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now()-start).count());
}

void worker::tryTosendUdpMessageToRemoteServer(std::pair<std::shared_ptr<std::array<uint8_t,2048>>,int>&&pair) {
    //流控背压机制,所有sesion都走这里所以做一下流控,再者同一socket不允许同时读写
    if (udpSocketWaitForSendDeque_.size()>2048) {
        return;//不允许总传输数量>2048 否则丢弃
    }
    bool isEmpty=udpSocketWaitForSendDeque_.empty();
    udpSocketWaitForSendDeque_.push_back(pair);
    if (isEmpty) {
        sendUdpMessageToRemoteServer();
    }
}

void worker::sendUdpMessageToRemoteServer() {
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
                sendUdpMessageToRemoteServer();
            }
    });
}


void worker::registerSession(std::unique_ptr<session,sessionDeleter> &sessionPtr) {
    //使用摘要算法制造16字节ID,存入absl哈希表进行匹配
    std::array<uint8_t, sessionIdSize> randomId=getSessionId();
    session* Ptr=sessionPtr.get();
    Ptr->setSessionId(randomId);//设置ID
    //将当前本地id session键值对存入哈希表,由于面向无连接状态,后续收到udp需要查找
    sessionMap_.emplace(randomId,std::move(sessionPtr));
    Ptr->start();
}

void worker::sessionDeleter::operator()(session* p)
{
    if(p)
    {
        p->~session();   // 调析构
    }
}

void worker::removeSessionFromHashMap(SessionId sessionId) {

}

void worker::removeSession(session* sessionPtr) {

    //sessionMap_.erase();
}

void worker::receiveUdpMessageFromRemoteServer() {
    remoteServerSocker_.async_receive(asio::buffer(udpReceiveBuffer,bufferSize),
    [this](boost::system::error_code ec,size_t byteHadRead) {
        if (ec) {
        LogE("出现问题");
        return;
        }
        int len=fastAes.doDecrypt(udpReceiveBuffer,byteHadRead);
        udpHeader *udpHeaderPoint=(udpHeader*)((char*)udpReceiveBuffer+keyAndIvOffSet);
        auto result=sessionMap_.find(udpHeaderPoint->sessionId_);
        if (result != sessionMap_.end()) {
            std::unique_ptr<session,sessionDeleter>&sessionPtr=result->second;
            sessionPtr->inputToKcp(udpHeaderPoint->data_,len-sessionIdSize);
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

std::array<uint8_t, worker::bufferSize>* worker::getMemory(int memorySize) {
    return (std::array<uint8_t, 2048> *)memoryPool_.mallocMemory(memorySize);
}

std::array<uint8_t, worker::bufferSize>* worker::getCPtrFunc(int memorySize) {
    return getMemory(memorySize);
}
//定制智能指针析构做法,减少代码量
std::shared_ptr<std::array<uint8_t, worker::bufferSize>> worker::getSharedPtrFunc(int memorySize) {
    return std::shared_ptr<std::array<uint8_t,bufferSize>>
    (getMemory(memorySize),[this](void* dataPoint) {
       freeMemory(dataPoint);
    });
}


