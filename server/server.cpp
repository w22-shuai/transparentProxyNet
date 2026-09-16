#include "server.h"

#include "session.h"
#include "worker.h"

void server::startThreadPool() {
    for (int i=0;i<threadSize_;++i) {
        //远端4个端口
        workerList_.emplace_back(std::unique_ptr<worker>(new worker(originPort+i)));
        workerList_[i]->start();
    }
}

std::unique_ptr<worker>& server::getThreadWorker() {
    int threadId=0;
    int sessionSize=0;
    for (int i=0;i<threadSize_;++i) {
        //由于每个worker上的session会销毁,为了负载均衡每次都要轮询一下,轮询开销是可以接受的
        if (workerList_[i]->sessionSize>sessionSize) {
            sessionSize=workerList_[i]->sessionSize;
            threadId=i;
        }
    }
    return workerList_[threadId];
}

server::server(int port, int threadSize):port_(port),ioCtx_(1),
checkServerAliveSocket_(ioCtx_),threadSize_(threadSize),
acceptor_(ioCtx_,tcp::endpoint(tcp::v4(),port_)) {}

void server::checkSeverAlive() {
    // checkServerAliveSocket_.async_send([this]
    //     (boost::system::error_code ec,size_t byteHadSend) {
    //     if (ec) {
    //         LogE("远端服务器出现问题");
    //         return;
    //     }
    //     std::shared_ptr<std::array<uint8_t,256>> buffer=std::make_shared<std::array<uint8_t,256>>();
    //     checkServerAliveSocket_.async_receive(asio::buffer(buffer.get(),buffer->size()),
    //         [this](boost::system::error_code ec,size_t byteHadRead) {
    //             if (ec) {
    //               LogE("远端服务器出现问题");
    //               return;
    //             }
    //             startThreadPool();
    //             work();
    //         });
    // });

}

void server::work() {
    std::unique_ptr<worker> &workerPtr=getThreadWorker();
    asio::io_context&ioCtx=workerPtr->getIoCtx();
    acceptor_.async_accept(ioCtx,[this,&ioCtx,&workerPtr]
        (boost::system::error_code ec,tcp::socket socket) mutable{
        if (!ec) {
            std::shared_ptr<session> sessionPtr=
                std::make_shared<session>(workerPtr,std::move(socket));
            //session可以在主线程创建但是后续处理必须依靠子线程,因此Post发往子线程
            asio::post(ioCtx.get_executor(),[&workerPtr,sessionPtr]() mutable{
                    workerPtr->registerSession(sessionPtr);
          });
       }
       work();
    });
}

void server::start() {
    checkSeverAlive();
}
