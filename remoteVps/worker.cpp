#include "worker.h"

#include "server.h"
#include "session.h"

worker::worker(int port,server&server):port_(port),server_(server),
ioCtx_(1),homeClientSocket_(ioCtx_.get_executor(),udp::endpoint(udp::v4(),port)),
kcpUpdateTimer_(ioCtx_.get_executor()){}


void worker::start() {
    threadPtr_=std::make_shared<std::thread>([this]() {
        udpReceiveBuffer=getMemory(bufferSize);//udp mtu2048完全够用
        listen();
        ++server_.sonThreadStatus_;
        server_.getConditionVariable().notify_one();
        ioCtx_.run();
    });
}

uint32_t worker::getClockMs() {
    using namespace std::chrono;
    static const steady_clock::time_point start=steady_clock::now();
    return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now()-start).count());
}

void worker::listen() {
    homeClientSocket_.async_receive_from(asio::buffer(udpReceiveBuffer,bufferSize),homeClienEndpoint_
    ,[this](boost::system::error_code ec,size_t bytesHadRead) {
        if (ec) {
        LogE("出现错误");
        }
        LogD("调试信息-->{}",homeClienEndpoint_.port());
        //delieverMessageFromClientHome
        int len=fastAes.doDecrypt(udpReceiveBuffer,bytesHadRead);
        udpHeader *udpHeaderPoint=(udpHeader*)udpReceiveBuffer;
        auto result=sessionMap_.find(udpHeaderPoint->sessionId_);
        if (result != sessionMap_.end()) {
            std::shared_ptr<session>sessionPtr=result->second;
            sessionPtr->inputToKcp(udpHeaderPoint->data_,len-sessionIdSize);
          }else {
              //构造新会话

              std::shared_ptr<session>sessionPtr=std::make_shared<session>
              (this,homeClienEndpoint_,tcp::socket(ioCtx_.get_executor()));
              registerSession(udpHeaderPoint->sessionId_,sessionPtr);
              sessionPtr->inputToKcp(udpHeaderPoint->data_,len-sessionIdSize);
          }
        listen();
    });
}

void worker::pushNewSessionToHeap(uint32_t time,std::shared_ptr<session>&&sessionPtr) {
    sessionHeap_.push(KcpUpdateNode{time,std::move(sessionPtr)});
}

void worker::updateSessionToHeap(uint32_t newDueAtMs,const std::shared_ptr<session> &sessionPtr) {
    sessionHeap_.update(sessionPtr->getHeapIndex(),KcpUpdateNode{newDueAtMs,sessionPtr});
}

void worker::tryTosendUdpMessageToRemoteServer(std::pair<std::shared_ptr<session>,
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
    std::pair<std::shared_ptr<session>,
    std::pair<std::shared_ptr<std::array<uint8_t,bufferSize>>,int>> &pair=
        udpSocketWaitForSendDeque_.front();
    homeClientSocket_.async_send_to(asio::buffer(pair.second.first.get(),pair.second.second),
        pair.first.get()->getEndPoint(),
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


void worker::KcpUpdateNode::setHeapIndex(uint32_t idx) {
    sessionPtr->setHeapIndex(idx);
}

uint32_t worker::KcpUpdateNode::getHeapIndex() const {
    return sessionPtr->getHeapIndex();
}

void worker::removeSessionFromHeap(uint32_t index) {
    sessionHeap_.remove(index);
}

void worker::rearmKcpUpdateTimer() {
    if (sessionHeap_.empty()) {
        return; //没有存活的session,不排定任何定时器,等下一次registerSession时再唤醒
    }
    uint32_t now=getClockMs();
    uint32_t dueAtMs=sessionHeap_.top().dueAtMs;
    uint32_t delayMs=(dueAtMs>now)?(dueAtMs-now):0;
    kcpUpdateTimer_.expires_after(std::chrono::milliseconds(delayMs));
    kcpUpdateTimer_.async_wait([this](const boost::system::error_code &ec) {
        onKcpUpdateTimer(ec);
    });
}

void worker::onKcpUpdateTimer(const boost::system::error_code &ec) {
    if (ec) {
        return; //定时器被显式取消(通常是rearmKcpUpdateTimer提前重排),旧的这次直接放弃
    }

    uint32_t now=getClockMs();
    //driveSessionTimeClock内部会经由updateTimeToWorkerHeap->updateSessionToHeap
    //把当前堆顶session原地更新为新的dueAtMs并重新sift,front()会随之改变,循环因此会终止
    while (!sessionHeap_.empty() && sessionHeap_.top().dueAtMs<=now) {
        std::shared_ptr<session> sessionPtr=sessionHeap_.top().sessionPtr;
        sessionPtr->driveSessionTimeClock(now);
    }
    rearmKcpUpdateTimer();
}

void worker::registerSession(SessionId&sessionId_,std::shared_ptr<session>&sessionPtr) {
    sessionMap_.emplace(sessionId_,sessionPtr);
    sessionPtr->setSessionId(sessionId_);
    uint32_t heapSize=sessionHeap_.size();
    sessionPtr->start();
    if (heapSize==0) {//说明才加进去的
        rearmKcpUpdateTimer();
    }
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

