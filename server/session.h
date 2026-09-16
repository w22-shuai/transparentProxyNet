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
    std::array<uint8_t, bufferSize> *remoteServerSocketBuffer_;

    std::deque<std::pair<std::shared_ptr<std::array<uint8_t,2048>>,int>> udpDataWaitForSendDeque;//kcp发送队列


    //由于单udp端口无连接性,需要在內令中设置id通过红黑树来遍历查询
    void receiveTcpFromWifiCilentMessage();
    void tryToSendToKcp(std::pair<std::shared_ptr<std::array<unsigned char, 2048>>, int> &&pair);
    void sendToKcp();
    void sendTcpToWifiClientMessage();
    std::array<uint8_t, bufferSize>* getCPtrFunc(int memorySize);
    std::shared_ptr<std::array<uint8_t, bufferSize>> getSharedPtrFunc(int memorySize);
    static int kcpCallBack(const char *buf, int len, ikcpcb *kcp, void *user);
    void tryToReadFromKcp();

public:
    enum class cmdStatus : uint32_t {
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
    static constexpr int cmdHeaderSize=20;
    static_assert(sizeof(cmdHeader) == cmdHeaderSize, "20字节对齐错误");


    cmdStatus currentCmdStatus_;//当前命令状态

    session()=delete;
    session(std::unique_ptr<worker> &worker, tcp::socket &&wifiClientSocket);
    ~session();
    void start();
    void setSessionId(std::array<uint8_t, 16> &sessionId){sessionId_=sessionId;};
    void inputToKcp(void *dataPtr, int len);



};