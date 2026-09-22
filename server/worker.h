#pragma once
#include "GlobalHeaders.h"
#include "../memoryPool/memoryPool.h"
#include "../priorityHeap/priorityHeap.hpp"
#include "AES.h"

class session;
class server;

class worker {    //一个worker一个ioCtx;
private:
    std::shared_ptr<std::thread> threadPtr_;
    int remoteServerPort_;
    memoryPool memoryPool_;
    asio::io_context ioCtx_;
    asio::executor_work_guard<asio::io_context::executor_type> workGuard_;
    udp::socket remoteServerSocker_;
    absl::flat_hash_map<std::array<uint8_t,16>,std::shared_ptr<session>> sessionMap_;//worker线程哈希表
    static constexpr int bufferSize=2048;
    std::array<uint8_t,bufferSize>* udpReceiveBuffer;
    FastAesGcmProcessor fastAes;
    struct KcpUpdateNode {
        uint32_t dueAtMs;
        std::shared_ptr<session> sessionPtr;

        bool operator<(const KcpUpdateNode &other) const {
            return static_cast<int32_t>(dueAtMs - other.dueAtMs) < 0;
        }
        void setHeapIndex(uint32_t idx);
        uint32_t getHeapIndex() const;
    };
    priorityHeap<KcpUpdateNode> sessionHeap_;
    std::deque<std::pair<std::shared_ptr<std::array<uint8_t,2048>>,int>> udpSocketWaitForSendDeque_;
    //udp待发送队列;
    asio::steady_timer kcpUpdateTimer_;

    server&server_;


    void removeSessionFromHeap(uint32_t index);
    void removeSessionFromHashMap(std::array<uint8_t, 16> sessionId);
    void cleanMemoryBlock();


public:
    static constexpr int sessionIdSize=16;
    static  constexpr int keyAndIvOffSet =44;
    typedef std::array<uint8_t, sessionIdSize> SessionId;
    std::atomic<uint32_t> sessionSize;//记录当前会话数量,用于server进行负载均衡
    struct udpHeader {
        SessionId sessionId_;
        char data_[];
    };

    worker(int remoteServerPort,server&server);
    worker()=delete;
    ~worker();
    void start();
    SessionId getSessionId();
    static uint32_t getClockMs();
    asio::io_context& getIoCtx(){return ioCtx_;};
    void registerSession(std::shared_ptr<session> &sessionPtr);//++sessionSize
    void tryTosendUdpMessageToRemoteServer(std::pair<std::shared_ptr<std::array<uint8_t,2048>>,int>&&pair);
    void sendUdpMessageToRemoteServer();
    void rearmKcpUpdateTimer();
    void onKcpUpdateTimer(const boost::system::error_code &ec);
    void receiveUdpMessageFromRemoteServer();
    void freeMemory(void*);
    void pushNewSessionToHeap(uint32_t time, std::shared_ptr<session> &&sessionPtr);
    void updateSessionToHeap(uint32_t newDueAtMs, const std::shared_ptr<session> &sessionPtr);

    void removeSession(const std::shared_ptr<session> &sessionPtr);

    std::array<uint8_t,2048> * getMemory(int memorySize);
    FastAesGcmProcessor& getFastAesGcm(){return fastAes;}
    std::array<uint8_t, bufferSize>* getCPtrFunc(int memorySize);
    //定制智能指针析构做法,减少代码量
    std::shared_ptr<std::array<uint8_t, bufferSize>> getSharedPtrFunc(int memorySize);



};
