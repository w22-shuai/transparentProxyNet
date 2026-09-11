#include <iostream>
#include <vector>


#include "memoryPool.h"



int main() {

    memoryPool a;
    std::vector<void *>b;
    for (int i=0;i<2048;++i) {
        b.push_back(a.mallocMemory());
    }
    for (int i=0;i<2048;++i) {
        a.freeMemory(b[i]);
    }
    a.freeOldMemoryBlock();
    a.freeOldMemoryBlock();
    return 0;
}