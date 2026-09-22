#include "server.h"

#include "session.h"
#include "worker.h"


server::server(int port, int threadSize):sonThreadStatus_(0),port_(port),ioCtx_(1),
checkServerAliveSocket_(ioCtx_),threadSize_(threadSize),
acceptor_(ioCtx_,tcp::endpoint(tcp::v4(),port_)) {}

server::~server() {}

void server::startThreadPool() {
    for (int i=0;i<threadSize_;++i) {
        //远端4个端口
        workerList_.emplace_back(std::unique_ptr<worker>(new worker(originPort+1+i,*this)));
        workerList_[i]->start();
    }
    //子线程准备好才开始监听
    {
        std::unique_lock lock(lock_);
        conditionVariable.wait(lock,[this]() {
            if (sonThreadStatus_==threadSize_) {
                return true;
            }
             return false;
        });
    }
    //LogD("主线程准备完毕");
    work();
}

std::unique_ptr<worker>& server::getThreadWorker() {
    int threadId=0;
    uint32_t sessionSize=workerList_[0]->sessionSize;
    for (int i=1;i<threadSize_;++i) {
        //由于每个worker上的session会销毁,为了负载均衡每次都要轮询一下,轮询开销是可以接受的
        if (workerList_[i]->sessionSize<sessionSize) {
            sessionSize=workerList_[i]->sessionSize;
            threadId=i;
        }
    }
    return workerList_[threadId];
}

void server::checkSeverAlive() {
    asio::ip::address ipAddress = asio::ip::make_address(ForeignServerIpaddr);
    checkServerAliveSocket_.async_connect(tcp::endpoint(ipAddress, originPort),
        [this](boost::system::error_code ec) {
             if (ec) {
               LogE("远端服务器tcp握手失败");
               return;
             }
             LogD("握手成功!准备启动子线程");
             startThreadPool();
    });
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
            ++workerPtr->sessionSize;//可能会出现线程并发量过大引起的负载不均衡问题,所以移到这里
            asio::post(ioCtx.get_executor(),[&workerPtr,sessionPtr]() mutable{
                    workerPtr->registerSession(sessionPtr);
          });
       }
       work();
    });
}

void server::start() {
    checkSeverAlive();
    ioCtx_.run();
}
