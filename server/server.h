#pragma once
#include "GlobalHeaders.h"



class worker;

class server {
private:

    int port_;//开放端口
    int threadSize_;//子线程大小
    asio::io_context ioCtx_;//主线程不用使用guard进行保护 主线程会一直有异步任务
    std::vector<std::unique_ptr<worker>> workerList_;
    tcp::socket checkServerAliveSocket_;
    tcp::acceptor acceptor_;
    void work();
    void checkSeverAlive();
    void startThreadPool();
    std::unique_ptr<worker> &getThreadWorker();

public:
    server(int port=10950,int threadSize_=4);
    void start();



};