#pragma once
#include "GlobalHeaders.h"



#include "worker.h"

class server {
private:
    int port_;
    int threadSize_;
    asio::io_context ioCtx_;
    tcp::acceptor acceptor_;//测活

    std::vector<std::unique_ptr<worker>> workerList_;

    std::mutex lock_;
    std::condition_variable conditionVariable_;

    void startThreadPool();//用来保证子线程服务开启完毕以后再开启主线程
    void checkAliveServer();;


public:
    server(int port=originPort,int threadSize=4);
    void start();
    std::atomic<int> sonThreadStatus_;
    std::condition_variable& getConditionVariable(){return conditionVariable_;}
};
