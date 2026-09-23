#pragma once
#include "GlobalHeaders.h"
#include "../memoryPool/memoryPool.h"
#include "../timeWheel/timeWheel.hpp"
#include "AES.h"

class session;
class server;

class worker {    //一个worker一个ioCtx;
public:
    class sessionDeleter{
    public:
        void operator()(session *p);
    };

private:
    std::shared_ptr<std::thread> threadPtr_;
    int remoteServerPort_;
    memoryPool memoryPool_;
    asio::io_context ioCtx_;
    asio::executor_work_guard<asio::io_context::executor_type> workGuard_;
    udp::socket remoteServerSocker_;
    absl::flat_hash_map<std::array<uint8_t,16>,std::unique_ptr<session,sessionDeleter>> sessionMap_;//worker线程哈希表
    static constexpr int bufferSize=2048;
    std::array<uint8_t,bufferSize>* udpReceiveBuffer;
    FastAesGcmProcessor fastAes;
    timeWheel<session> timeWheel_;


    std::deque<std::pair<std::shared_ptr<std::array<uint8_t,2048>>,int>> udpSocketWaitForSendDeque_;
    //udp待发送队列;
    asio::steady_timer kcpUpdateTimer_;

    server&server_;



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

    void startTimeWheel();

    SessionId getSessionId();
    static uint32_t getClockMs();
    asio::io_context& getIoCtx(){return ioCtx_;};
    timeWheel<session>& getTimeWheel(){return timeWheel_;}
    void registerSession(std::unique_ptr<session, sessionDeleter> &sessionPtr);//++sessionSize
    void tryTosendUdpMessageToRemoteServer(std::pair<std::shared_ptr<std::array<uint8_t,2048>>,int>&&pair);
    void sendUdpMessageToRemoteServer();
    void receiveUdpMessageFromRemoteServer();
    void freeMemory(void*);
    void removeSession(session* sessionPtr);

    std::array<uint8_t,2048> * getMemory(int memorySize);
    FastAesGcmProcessor& getFastAesGcm(){return fastAes;}
    std::array<uint8_t, bufferSize>* getCPtrFunc(int memorySize);
    //定制智能指针析构做法,减少代码量
    std::shared_ptr<std::array<uint8_t, bufferSize>> getSharedPtrFunc(int memorySize);



};
