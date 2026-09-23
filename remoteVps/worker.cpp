#include "worker.h"

#include "server.h"
#include "session.h"

worker::worker(int port,server&server):port_(port),server_(server),
ioCtx_(1),homeClientSocket_(ioCtx_.get_executor(),udp::endpoint(udp::v4(),port)),
kcpUpdateTimer_(ioCtx_.get_executor()),workGuard_(asio::make_work_guard(ioCtx_)){}

worker::~worker() {
    freeMemory(udpReceiveBuffer);
}


void worker::start() {
    threadPtr_=std::make_shared<std::thread>([this]() {
        udpReceiveBuffer=getMemory(bufferSize);//udp mtu2048完全够用
        listen();
        ++server_.sonThreadStatus_;
        timeWheel_.operatorFunction_.ctwCallBack_=session::driveSessionTimeClock;
        timeWheel_.operatorFunction_.rtCallBack_=session::doCloseSession;
        startTimeWheel();
        server_.getConditionVariable().notify_one();
        ioCtx_.run();
        LogD("线程销毁");
    });
}

uint32_t worker::getClockMs() {
    using namespace std::chrono;
    static const steady_clock::time_point start=steady_clock::now();
    return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now()-start).count());
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

timeWheel<session>& worker::getTimeWheel() {
    return timeWheel_;
}

void worker::sessionDeleter::operator()(session* p)
{
    if(p)
    {
        p->~session();   // 调析构
    }
}

void worker::listen() {
    homeClientSocket_.async_receive_from(asio::buffer(udpReceiveBuffer,bufferSize),homeClienEndpoint_
    ,[this](boost::system::error_code ec,size_t bytesHadRead) {
        if (ec) {
        LogE("出现错误");
        listen();
        }
        //delieverMessageFromClientHome
        int len=fastAes.doDecrypt(udpReceiveBuffer,bytesHadRead);
        udpHeader *udpHeaderPoint=(udpHeader*)((char*)udpReceiveBuffer+keyAndIvOffSet);
        auto result=sessionMap_.find(udpHeaderPoint->sessionId_);
        if (result != sessionMap_.end()) {
            std::unique_ptr<session,sessionDeleter>&sessionPtr=result->second;
            sessionPtr->inputToKcp(udpHeaderPoint->data_,len-sessionIdSize);
          }else {
              //构造新会话
              session* Ptr=new ((session*)getTimeWheel().registerTask(
                    getCPtrFunc(timeWheel<session>::structTaskSize_)))
              session(this,homeClienEndpoint_,tcp::socket(ioCtx_.get_executor()));
              std::unique_ptr<session,sessionDeleter>sessionPtr(Ptr);
              registerSession(udpHeaderPoint->sessionId_,sessionPtr);
              Ptr->inputToKcp(udpHeaderPoint->data_,len-sessionIdSize);
          }
        listen();
    });
}


void worker::tryTosendUdpMessageToRemoteServer(std::pair<udp::endpoint,
    std::pair<std::shared_ptr<std::array<uint8_t,bufferSize>>,int>>&&pair) {
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
    std::pair<udp::endpoint,
    std::pair<std::shared_ptr<std::array<uint8_t,bufferSize>>,int>> &pair=
        udpSocketWaitForSendDeque_.front();
    homeClientSocket_.async_send_to(asio::buffer(pair.second.first->data(),pair.second.second),
        pair.first,
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



void worker::registerSession(SessionId&sessionId_,std::unique_ptr<session,sessionDeleter> &sessionPtr) {
    session*ptr=sessionPtr.get();
    sessionMap_.emplace(sessionId_,std::move(sessionPtr));
    ptr->setSessionId(sessionId_);
    ptr->start();
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

