#pragma once

#include "GlobalHeaders.h"




class worker;

class session:public std::enable_shared_from_this<session>{
private:

    std::unique_ptr<worker>& worker_;
    /*该session属于的worker,为的是调用该线程唯一udp
     socket做准备,由于智能指针的不可复制性,采用引用*/

    tcp::socket wifiClientSocket_;
    ikcpcb *kcp_;

    //由于单udp端口无连接性,需要在內令中设置id通过红黑树来遍历查询
    void receiveTcpFromWifiCilentMessage();
    void sendTcpToWifiClientMessage();
    std::shared_ptr<std::array<uint8_t, 2048>> getSharedPtrFunc(int memorySize);
    void receiveFromKcp();
    void doSendToKcp();
    static int kcpCallBack(const char *buf, int len, ikcpcb *kcp, void *user);

public:
    session()=delete;
    session(std::unique_ptr<worker> &worker, tcp::socket &&wifiClientSocket);
    ~session();
    void start();

    int localSessionId_;
    int remoteSessionId_;
    std::deque<std::pair<std::shared_ptr<std::array<uint8_t,2048>>,int>> udpDataWaitForSendDeque;//kcp发送队列
};