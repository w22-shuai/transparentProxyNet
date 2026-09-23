#pragma once
#include <array>
#include <memory>




template <typename T>
class timeWheel{
private:
    struct task{
        struct assistStruct {
            task* next_;
            task* ahead_;
        };
        assistStruct assistStruct_;
        T node_;
    };
    struct taskHeader {
        task* next_;//next一定要在前面
        int currentTaskSize_;
        taskHeader():next_(nullptr),currentTaskSize_(0){};
    };//时间片头,后续维护的时候用于负载均衡,如果此时时间片任务过多则截取一部分丢到下一个时间片中
    struct operatorFunction{
        using doCurrentTimeWheelCallBack=uint32_t (*)(void*);
        using doremoveTaskCallBack=void (*)(void*);
        doCurrentTimeWheelCallBack ctwCallBack_;
        doremoveTaskCallBack rtCallBack_;
    };

    static constexpr int timeWheelLength_=1000;//12000字节 10kb左右 总长3000ms

    int currentTaskNumber_;
    std::array<taskHeader,timeWheelLength_> timeSegmentList_{};




public:
    static constexpr int deltaTime_=3;//时间差,这里我们设置为3ms的时间差
    static constexpr int structTaskSize_=sizeof(task);

    operatorFunction operatorFunction_;

    timeWheel():currentTaskNumber_(0) {

    };
    void removeTask(T* node) {
        task* taskPoint=(task*)((char*)node-sizeof(typename task::assistStruct));
        //若taskPoint->assistStruct_.ahead_->assistStruct_.next_为首指针显然会出问题,因此必须对齐next
        taskPoint->assistStruct_.ahead_->assistStruct_.next_=taskPoint->assistStruct_.next_;
        if (taskPoint->assistStruct_.next_) {          // 补上判空,尾节点时不需要更新"下一个"的ahead_
            taskPoint->assistStruct_.next_->assistStruct_.ahead_=taskPoint->assistStruct_.ahead_;
        }
        //断链准备回收这块内存
        operatorFunction_.ctwCallBack_(&taskPoint->node_);//兹定60s后销毁map里的session,算是TIME_WAIT
    }
    void* registerTask(void*memoryPoint) {
        //传入内存池内存
        //返回期望T的类型指针
         task* taskPoint=(task*)memoryPoint;
         return  &taskPoint->node_;
    }
    void mountTask(T* node,uint32_t next) {
        int ticks = next / deltaTime_;
        if (ticks < 1) {
            ticks = 1;
        }
        int targetTaskNumber_ = (currentTaskNumber_ + ticks) % timeWheelLength_;
        task* taskPoint=timeSegmentList_[targetTaskNumber_].next_;
        task* currentTaskPoint=(task*)((char*)node-sizeof(typename task::assistStruct));
        if (taskPoint==nullptr) {
            currentTaskPoint->assistStruct_.next_=taskPoint;
            currentTaskPoint->assistStruct_.ahead_=(task*)&timeSegmentList_[targetTaskNumber_];
            timeSegmentList_[targetTaskNumber_].next_=currentTaskPoint;
            return ;
        }
        taskPoint->assistStruct_.ahead_->assistStruct_.next_=currentTaskPoint;
        currentTaskPoint->assistStruct_.next_=taskPoint;
        currentTaskPoint->assistStruct_.ahead_=taskPoint->assistStruct_.ahead_;
        taskPoint->assistStruct_.ahead_=currentTaskPoint;
    }
    void doCurrentTimeWheel() {
        task*currentTask=timeSegmentList_[currentTaskNumber_].next_;
        timeSegmentList_[currentTaskNumber_].next_=nullptr;
        while (currentTask!=nullptr) {
            task*assistPoint=currentTask->assistStruct_.next_;
            int nextTime=operatorFunction_.ctwCallBack_(&currentTask->node_);
            if (nextTime > 0) { // 通常返回0代表不再定时，需要判断
                mountTask(&currentTask->node_, nextTime);
            }
            currentTask=assistPoint;
        }
        ++currentTaskNumber_;
        if (currentTaskNumber_>=1000) {
            currentTaskNumber_=0;
        }//回到初始时间片
    }
};
