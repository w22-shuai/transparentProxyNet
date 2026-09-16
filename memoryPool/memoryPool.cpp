#include "memoryPool.h"



memoryPool::memoryPool(int n):memoryBlockSize(BlockSize*n)
{
    //为了高性能存储把头和数据放在一个内存块中,便于缓存利用
    int beginSize=256;
    for (int i =0;i<nodePointHeadLinkerHeadLength;++i) {
        int nodeSize=beginSize+sizeof(headPointAndData::head);//256+头节点大小
        int numberOfNode=(memoryBlockSize-alignedLinker)/nodeSize-1;
        void*memoryAddr=aligned_alloc(16,memoryBlockSize);
        //定位new
        nodePointHeadLinkerHead[i]=new(memoryAddr)nodePointLinkerHead
        ((char*)memoryAddr+alignedLinker,numberOfNode,nodeSize);
        beginSize=beginSize*2;
    }


}

void memoryPool::getNewMemoryBlock(nodePointLinker*point,nodePointLinkerHead*headPoint)
{
    //申请新内存块
    void*memoryAddr=aligned_alloc(16,memoryBlockSize);
    //内存对齐存在疑问
    point->nextPoint=new(memoryAddr)nodePointLinker((char*)memoryAddr+alignedLinker,
        headPoint->numberOfNode,headPoint->nodeSize);
}

void memoryPool::freeOldMemoryBlock()  {
   for (int i =0;i<nodePointHeadLinkerHeadLength;++i) {
       nodePointLinker *assistPoint=&nodePointHeadLinkerHead[i]->point;
       nodePointLinker *point=assistPoint->nextPoint;//第一个不能被清除
       int numbeOfNode=nodePointHeadLinkerHead[i]->numberOfNode;
       while (point!=nullptr) {
           if (point->headPoint.currentNodeNumber==numbeOfNode) {
               //释放链表中node个数为512的节点
               assistPoint->nextPoint=point->nextPoint;
               point->~nodePointLinker();
               free(point);
               point=assistPoint->nextPoint;
           }else {
               point=point->nextPoint;
               assistPoint=assistPoint->nextPoint;
           }
       }
   }
}

void* memoryPool::mallocMemory(int memorySize) {
    //返回申请内存的位置
    //判定区间从而确定头节点位置
    int i=(0x3FFA4 >> ((memorySize >> 8) * 2)) & 3;//整型自然截断
    nodePointLinker *assistPoint=&nodePointHeadLinkerHead[i]->point;
    nodePointLinker *point=assistPoint;
    while (true) {
        if (assistPoint==nullptr) {
            getNewMemoryBlock(point,nodePointHeadLinkerHead[i]);
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
