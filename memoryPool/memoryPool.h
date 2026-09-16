#pragma once
#include <cstdint>
#include <iostream>

static constexpr long long  BlockSize = 1024LL * 1024;

class memoryPool {
private:
    long long memoryBlockSize;
    struct nodePoint{//用来存放下一个链表节点
        nodePoint* nextNodePoint;
    };//元素节点
    struct nodePointHead{//用来存放下一个链表节点
        void*memoryBlockPoint;//用来申请的大块内存
        int currentNodeNumber;//当前节点数量
        nodePoint* nextNodePoint;
        nodePointHead()=delete;
        nodePointHead(void*memoryBlockPoint_):
        memoryBlockPoint(memoryBlockPoint_),
        currentNodeNumber(0),nextNodePoint((nodePoint*)memoryBlockPoint) {
        }
    };//头节点
    struct nodePointLinker{
        nodePointHead headPoint;
        nodePointLinker*nextPoint;
        nodePointLinker()=delete;
        nodePointLinker(void*memoryBlockPoint,int numberOfNode,int nodeSize):
        nextPoint(nullptr),headPoint(memoryBlockPoint) {
            headPoint.currentNodeNumber=numberOfNode;
            nodePoint* assistpoint=(nodePoint*)headPoint.memoryBlockPoint;
            for (int n=0;n<numberOfNode;++n) {
                assistpoint->nextNodePoint=(nodePoint*)((char*)assistpoint+nodeSize);
                assistpoint=assistpoint->nextNodePoint;
            }
            assistpoint->nextNodePoint=nullptr;
        };
        ~nodePointLinker() {};
    };//链表
    struct nodePointLinkerHead {
        int numberOfNode;
        int nodeSize;
        nodePointLinker point;
        nodePointLinkerHead(void* memoryBlockPoint,int numberOfNode,int nodeSize):numberOfNode(numberOfNode),
        nodeSize(nodeSize),point(memoryBlockPoint,numberOfNode,nodeSize){}
    };
    struct headPointAndData {
        struct head {
            nodePointLinker *point;
            uint64_t padding;//8字节填充
        };
        head headData;
        char dataPoint[];//方便强转2048字节
    };//用于定位
    static constexpr int alignedLinker = (sizeof(nodePointLinkerHead) + 15) & ~15;
    //保证16字节对齐
    static constexpr int nodePointHeadLinkerHeadLength=4;
    nodePointLinkerHead* nodePointHeadLinkerHead[nodePointHeadLinkerHeadLength];//装第一个节点,这个节点不会被删除
    void getNewMemoryBlock(nodePointLinker *point, nodePointLinkerHead *headPoint);

public:
    memoryPool(int n=1);
    memoryPool(const memoryPool&)=delete;
    memoryPool(memoryPool&&)=delete;
    ~memoryPool(){};
    void* mallocMemory(int memorySize);
    void freeMemory(void *point);
    void freeOldMemoryBlock();
};