#pragma once

#include "GlobalHeaders.h"
#include "worker.h"





class session:public std::enable_shared_from_this<session>{
private:

    std::unique_ptr<worker>& worker_;
    /*该session属于的worker,为的是调用该线程唯一udp
     socket做准备,由于智能指针的不可复制性,采用引用*/
    worker::SessionId sessionId_;
    /*由于我们采用16字节随机值策略
     所以当前会话id和远程会话id可以共用这一个sessionId
    */
    tcp::socket wifiClientSocket_;
    ikcpcb *kcp_;
    static constexpr int bufferSize=2048;
    static constexpr int bufferPaddingSize=128;//用于kcpCallBack函数中
    std::array<uint8_t, bufferSize> *wifiClientSocketBuffer_;
    //TODO remoteServerSocketBuffer_是否被需要
    uint32_t currentHeapNumber_;

    std::deque<std::pair<std::shared_ptr<std::array<uint8_t,2048>>,int>> tcpDataWaitForSendDeque_;

    static constexpr int kcpSendWindowHighWaterMark=128;//kcp队列拥堵最大状态
    bool trafficBusy_;//kcp流量控制receiveTcpFromWifiCilentMessage函数

    //由于单udp端口无连接性,需要在內令中设置id通过红黑树来遍历查询

    void receiveTcpFromWifiCilentMessage();

    void checkTrafficStatus();

    void sendTcpToWifiClientMessage();
    static int kcpCallBack(const char *buf, int len, ikcpcb *kcp, void *user);
    void tryToReadFromKcp();
    void trySendTcpToWifiClientMessage(std::pair<std::shared_ptr<std::array<uint8_t,bufferSize>>,int>&& pair);



public:
    enum class cmdStatus : uint32_t {
        //保证4字节
        normal=0x00,
        newSession=0x01,
        removeSession=0x02,
        checkSessionAlive=0x03
    };

    #pragma pack(push, 1)
    struct cmdHeader {
        worker::SessionId sessionId_;
        cmdStatus cmd_;
    };
    #pragma pack(pop)
    static constexpr int cmdHeaderSize=sizeof(cmdHeader);
    static_assert(sizeof(cmdHeader) == 20, "20字节对齐错误");


    cmdStatus currentCmdStatus_;//当前命令状态

    session()=delete;
    session(std::unique_ptr<worker> &worker, tcp::socket &&wifiClientSocket);
    ~session();
    void start();
    void closeSession();
    void timeToWork(uint32_t now);
    void updateTimeToWorkerHeap(uint32_t now);
    void setSessionId(std::array<uint8_t, 16> &sessionId){sessionId_=sessionId;};
    void inputToKcp(void *dataPtr, int len);
};