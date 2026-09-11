#pragma once
#include <cstdlib>
#include <iostream>


#define BlockSize (1024*1024)
#define OffSet (2048 + sizeof(headPointAndData::head))



class memoryPool {
private:

    long long memoryBlockSize;
    int nodeSize;//节点大小
    int numberOfNode;//节点个数
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
        nodePointLinker(void*memoryBlockPoint,memoryPool*point):
        nextPoint(nullptr),headPoint(memoryBlockPoint) {
            headPoint.currentNodeNumber=point->numberOfNode;
            nodePoint* assistpoint=(nodePoint*)headPoint.memoryBlockPoint;
            for (int n=0;n<point->numberOfNode;++n) {
                assistpoint->nextNodePoint=(nodePoint*)((char*)assistpoint+OffSet);
                assistpoint=assistpoint->nextNodePoint;
            }
            assistpoint->nextNodePoint=nullptr;
        };
        ~nodePointLinker() {};
    };//链表
    struct headPointAndData {
        struct head {
            nodePointLinker *point;
            __uint64_t padding;//8字节填充
        };
        head headData;
        char dataPoint[];//方便强转2048字节
    };//用于定位
    static constexpr int alignedLinker = (sizeof(nodePointLinker) + 15) & ~15;
    nodePointLinker*nodePointHeadLinkerHead;//装第一个节点,这个节点不会被删除
    void getNewMemoryBlock(nodePointLinker *point);



public:
    memoryPool(int n=1);
    memoryPool(const memoryPool&)=delete;
    memoryPool(memoryPool&&)=delete;
    void* mallocMemory();
    void freeMemory(void *point);
    void freeOldMemoryBlock();
};