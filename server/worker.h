#pragma once
#include "GlobalHeaders.h"
#include "../memoryPool/memoryPool.h"
#include "../priorityHeap/priorityHeap.h"
#include "AES.h"


class session;



class worker {
private:

    //一个worker一个ioCtx;
    std::shared_ptr<std::thread> threadPtr_;
    int remoteServerPort_;
    memoryPool memoryPool_;
    asio::io_context ioCtx_;
    asio::executor_work_guard<asio::io_context::executor_type> workGuard_;
    udp::socket remoteServerSocker_;//udp尝试连接,其实没有连接此操作为绑定端口而已
    absl::flat_hash_map<std::array<uint8_t,16>,std::shared_ptr<session>> sessionMap_;
    //worker线程哈希表
    static constexpr int bufferSize=2048;
    std::array<uint8_t,bufferSize>* udpReceiveBuffer;
    FastAesGcmProcessor fastAes;

    priorityHeap<std::pair<uint32_t,std::shared_ptr<session>>> sessionHeap_;

    std::deque<std::pair<std::shared_ptr<std::array<uint8_t,2048>>,int>> udpSocketWaitForSendDeque_;
    //udp待发送队列;

    void cleanMemoryBlock();

public:
    typedef std::array<uint8_t, 16> SessionId;
    std::atomic<uint32_t> sessionSize;//记录当前会话数量,用于server进行负载均衡

    worker(int remoteServerPort);
    worker()=delete;
    ~worker();
    void start();
    SessionId getSessionId();
    static uint32_t getClockMs();
    asio::io_context& getIoCtx(){return ioCtx_;};
    void registerSession(std::shared_ptr<session> &sessionPtr);//++sessionSize
    void tryTosendUdpMessageToRemoteServer(std::pair<std::shared_ptr<std::array<uint8_t,2048>>,int>&&pair);
    void sendUdpMessageToRemoteServer();
    void receiveUdpMessageFromRemoteServer();
    void freeMemory(void*);
    void pushNewSessionToHeap(uint32_t time, std::shared_ptr<session> &&sessionPtr);



    std::array<uint8_t,2048> * getMemory(int memorySize);
    FastAesGcmProcessor& getFastAesGcm(){return fastAes;}
    std::array<uint8_t, bufferSize>* getCPtrFunc(int memorySize);
    //定制智能指针析构做法,减少代码量
    std::shared_ptr<std::array<uint8_t, bufferSize>> getSharedPtrFunc(int memorySize);
};
