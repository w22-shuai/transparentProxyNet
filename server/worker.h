#pragma once
#include "GlobalHeaders.h"
#include "../memoryPool/memoryPool.h"

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
    std::vector<std::shared_ptr<session>> sessionList_;//TODO


public:
    worker(int remoteServerPort);
    worker()=delete;
    ~worker();
    void start();
    std::atomic<int> sessionSize;//记录当前会话数量,用于server进行负载均衡
    asio::io_context& getIoCtx(){return ioCtx_;};
    void registerSession(std::shared_ptr<session> &sessionPtr);//++sessionSize
    int getPort(){return remoteServerPort_;};
    void sendUdpMessageToRemoteServer();
    void receiveUdpMessageFromRemoteServer();
    void freeMemory(void*);
    std::array<uint8_t,2048> * getMemory(int memorySize);
};
