#include "../memoryPool.h"

#include <iostream>

memoryPool::memoryPool(int n):memoryBlockSize(BlockSize*n),
nodeSize(OffSet),numberOfNode(BlockSize*n/nodeSize-2)//可以少量剩余空间不使用 但是不能额外占用外部空间
{
    //为了高性能存储
    void*memoryAddr=malloc(memoryBlockSize);
    nodePointHeadLinkerHead=new(memoryAddr)nodePointLinker((char*)memoryAddr+alignedLinker,this);
    //定位new
}

void memoryPool::getNewMemoryBlock(nodePointLinker*point){
    //申请新内存块
    void*memoryAddr=malloc(memoryBlockSize);
    //内存对齐存在疑问
    point->nextPoint=new(memoryAddr)nodePointLinker((char*)memoryAddr+alignedLinker,this);
}

void memoryPool::freeOldMemoryBlock()  {
    nodePointLinker *assistPoint=nodePointHeadLinkerHead;
    nodePointLinker *point=nodePointHeadLinkerHead->nextPoint;//第一个不能被清除
    while (point!=nullptr) {
        if (point->headPoint.currentNodeNumber==numberOfNode) {
            //释放链表中node个数为512的节点
            assistPoint->nextPoint=point->nextPoint;
            free(point);
            point=assistPoint->nextPoint;
        }else {
            point=point->nextPoint;
            assistPoint=assistPoint->nextPoint;
        }
    }
}

void* memoryPool::mallocMemory() {
    //返回申请内存的位置
    nodePointLinker *assistPoint=nodePointHeadLinkerHead;
    nodePointLinker *point=nodePointHeadLinkerHead;
    while (true) {
        if (assistPoint==nullptr) {
            getNewMemoryBlock(point);
            assistPoint=point;
        }
        if (assistPoint->headPoint.currentNodeNumber!=0) {
            --assistPoint->headPoint.currentNodeNumber;
            nodePoint *retPoint=assistPoint->headPoint.nextNodePoint;
            assistPoint->headPoint.nextNodePoint=retPoint->nextNodePoint;
            ((headPointAndData*)retPoint)->headData.point=assistPoint;
            return ((headPointAndData*)retPoint)->dataPoint;
        }
        point=assistPoint;
        assistPoint=assistPoint->nextPoint;
    }
}

void memoryPool::freeMemory(void *point){
    headPointAndData *assistPoint=
        (headPointAndData *)((char*)point-sizeof(headPointAndData::head));
    ++assistPoint->headData.point->headPoint.currentNodeNumber;
    point=assistPoint->headData.point->headPoint.nextNodePoint;
    assistPoint->headData.point->headPoint.nextNodePoint=(nodePoint*)assistPoint;
    ((nodePoint *)assistPoint)->nextNodePoint=(nodePoint *)point;
}
