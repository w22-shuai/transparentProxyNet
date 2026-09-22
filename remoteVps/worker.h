#pragma once
#include  "GlobalHeaders.h"
#include "../memoryPool/memoryPool.h"
#include "../priorityHeap/priorityHeap.hpp"
#include "AES.h"


class server;
class session;



class worker {
private:
    std::atomic<uint32_t> sessionSize_;
    server&server_;
    int port_;
    asio::io_context ioCtx_;
    asio::executor_work_guard<asio::io_context::executor_type> workGuard_;
    memoryPool memoryPool_;
    std::shared_ptr<std::thread> threadPtr_;
    absl::flat_hash_map<std::array<uint8_t,16>,std::shared_ptr<session>> sessionMap_;//worker线程哈希表
    static constexpr int bufferSize=2048;
    std::array<uint8_t,bufferSize>* udpReceiveBuffer;
    udp::socket homeClientSocket_;
    udp::endpoint homeClienEndpoint_;//临时容器
    std::deque<std::pair<std::shared_ptr<session>,
    std::pair<std::shared_ptr<std::array<uint8_t,bufferSize>>,int>>> udpSocketWaitForSendDeque_;
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
    asio::steady_timer kcpUpdateTimer_;

    priorityHeap<KcpUpdateNode> sessionHeap_;

    void listen();

    void removeSessionFromHeap(uint32_t index);
    void rearmKcpUpdateTimer();
    void onKcpUpdateTimer(const boost::system::error_code &ec);
    void registerSession(std::array<unsigned char, 16> &sessionId_, std::shared_ptr<session> &sessionPtr);

    void cleanMemoryBlock();


public:
    static constexpr int sessionIdSize=16;
    static  constexpr int keyAndIvOffSet =44;
    typedef std::array<uint8_t, sessionIdSize> SessionId;
    struct udpHeader {
        SessionId sessionId_;
        char data_[];
    };
    worker(int port,server&server);
    ~worker();

    void start();

    static uint32_t getClockMs();

    std::array<uint8_t,bufferSize> * getMemory(int memorySize);
    std::array<uint8_t, bufferSize> * getCPtrFunc(int buffer_size);
    std::shared_ptr<std::array<uint8_t, worker::bufferSize>> getSharedPtrFunc(int memorySize);
    void pushNewSessionToHeap(uint32_t time, std::shared_ptr<session> &&sessionPtr);
    FastAesGcmProcessor& getFastAesGcm() {
        return fastAes;
    }
    void updateSessionToHeap(uint32_t newDueAtMs, const std::shared_ptr<session> &sessionPtr);
    void freeMemory(void *dataPtr);
    void tryTosendUdpMessageToRemoteServer(std::pair<std::shared_ptr<session>, std::pair<std::shared_ptr<std::array<unsigned char, 2048>>, int>> &&pair);

    void sendUdpMessageToRemoteServer();
};