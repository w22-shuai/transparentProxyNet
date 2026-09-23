#pragma once
#include  "GlobalHeaders.h"
#include "../memoryPool/memoryPool.h"
#include "../timeWheel/timeWheel.hpp"
#include "AES.h"


class server;
class session;



class worker {
public:
    static constexpr int sessionIdSize=16;
    static  constexpr int keyAndIvOffSet =44;
    typedef std::array<uint8_t, sessionIdSize> SessionId;
    class sessionDeleter{
    public:
        void operator()(session *p);
    };

private:
    std::atomic<uint32_t> sessionSize_;
    server&server_;
    int port_;
    asio::io_context ioCtx_;
    asio::executor_work_guard<asio::io_context::executor_type> workGuard_;
    memoryPool memoryPool_;
    std::shared_ptr<std::thread> threadPtr_;
    absl::flat_hash_map<std::array<uint8_t,16>,std::unique_ptr<session,sessionDeleter>> sessionMap_;//worker线程哈希表
    static constexpr int bufferSize=2048;
    std::array<uint8_t,bufferSize>* udpReceiveBuffer;
    udp::socket homeClientSocket_;
    udp::endpoint homeClienEndpoint_;//临时容器
    std::deque<std::pair<udp::endpoint,
    std::pair<std::shared_ptr<std::array<uint8_t,bufferSize>>,int>>> udpSocketWaitForSendDeque_;
    FastAesGcmProcessor fastAes;
    timeWheel<session> timeWheel_;


    asio::steady_timer kcpUpdateTimer_;



    void listen();
    void registerSession(SessionId &sessionId_, std::unique_ptr<session, sessionDeleter> &sessionPtr);
    void cleanMemoryBlock();


public:


    struct udpHeader {
        SessionId sessionId_;
        char data_[];
    };
    worker(int port,server&server);
    ~worker();
    void start();
    static uint32_t getClockMs();
    void startTimeWheel();
    timeWheel<session> &getTimeWheel();
    std::array<uint8_t,bufferSize> * getMemory(int memorySize);
    std::array<uint8_t, bufferSize> * getCPtrFunc(int buffer_size);
    std::shared_ptr<std::array<uint8_t, worker::bufferSize>> getSharedPtrFunc(int memorySize);

    FastAesGcmProcessor& getFastAesGcm() {
        return fastAes;
    }

    void freeMemory(void *dataPtr);
    void tryTosendUdpMessageToRemoteServer(std::pair<udp::endpoint, std::pair<std::shared_ptr<std::array<unsigned char, 2048>>, int>> &&pair);

    void sendUdpMessageToRemoteServer();
};