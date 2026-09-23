#pragma once
#include  "GlobalHeaders.h"
#include "worker.h"


class session{
private:
    ikcpcb*kcp_;
    worker *worker_;
    udp::endpoint homeClienEndpoint_;
    tcp::socket targetServerTcpSocket_;
    bool trafficBusy_;//kcp流量控制receiveTcpFromWifiCilentMessage函数
    bool enableToSendToTargetServer_;
    static constexpr int bufferSize=2048;
    static constexpr int bufferPaddingSize=128;//用于kcpCallBack函数中
    std::array<uint8_t, bufferSize> *targetServerTcpSocketBuffer_;
    static constexpr int kcpSendWindowHighWaterMark=128;//kcp队列拥堵最大状态
    worker::SessionId sessionId_;
    uint32_t currentHeapNumber_;
    std::deque<std::pair<std::shared_ptr<std::array<uint8_t,bufferSize>>,int>> tcpDataWaitForSendDeque_;
    void trySendTcpToTargetServerMessage(std::pair<std::shared_ptr<std::array<uint8_t, bufferSize>>, int> &&pair);
    void sendTcpToTargetServerMessage();
    void receiveTcpFromTargetServerMessage();
    void checkTrafficStatus();
    void tryToReadFromKcp();
    void handShakeWithtargetServer(tcp::endpoint &ep);
    static int kcpCallBack(const char *buf, int len, ikcpcb *kcp, void *user);

public:
    enum class cmdStatus : uint32_t {
        //保证4字节
        newSession=0x00,
        normal=0x01,
        removeSession=0x02,
        checkSessionAlive=0x03
    };
    static constexpr int cmdStatusSize=sizeof(cmdStatus);

#pragma pack(push, 1)
    struct statusAndData {
        cmdStatus status_;
        struct ipAndPort {
            uint32_t ip_;
            uint16_t port_;
        };
        union {
            ipAndPort ipAndPort_;
            char assistPoint[0];
        };
    };
#pragma pack(pop)
    static constexpr int statusAndDataSize=sizeof(statusAndData);
    static_assert(sizeof(statusAndData) == 10, "6字节对齐错误");

    bool close_;//当前session是否关闭

    cmdStatus currentCmdStatus_;//当前命令状态

    session()=delete;
    ~session();
    session(worker *worker,udp::endpoint& endpoint,tcp::socket&&targetServerTcpSocket);
    void inputToKcp(void *dataPtr, int len);
    void setSessionId(worker::SessionId &sessionId){sessionId_=sessionId;}
    void start();


    void closeSession();

    static void doCloseSession(void *current);
    static uint32_t driveSessionTimeClock(void *current);
    udp::endpoint &getEndPoint(){return homeClienEndpoint_;}
};