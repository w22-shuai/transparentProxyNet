#include "server.h"




server::server(int port, int threadSize):port_(port),ioCtx_(1),
threadSize_(threadSize),acceptor_(ioCtx_,tcp::endpoint(tcp::v4(),port_)) {}

void server::start() {
    checkAliveServer();
    startThreadPool();
    ioCtx_.run();
}



void server::startThreadPool() {
    for (int i=0;i<threadSize_;++i) {
        workerList_.emplace_back(std::unique_ptr<worker>(new worker(originPort+1+i,*this)));
        workerList_[i]->start();
    }
    {
        std::unique_lock lock(lock_);
        conditionVariable_.wait(lock,[this]() {
            if (sonThreadStatus_==threadSize_) {
                return true;
            }
             return false;
        });
    }
}


void server::checkAliveServer() {
    acceptor_.async_accept(ioCtx_,[this](boost::system::error_code ec,tcp::socket socket) {
        if (!ec) {
            LogD("收到新连接");
            //可以关闭此套接字
           }
        checkAliveServer();
    });
}


