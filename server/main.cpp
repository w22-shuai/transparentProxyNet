#include "GlobalHeaders.h"






int main() {
    initlog();
    int memorySize=511;
    int i=(0x3FFA4 >> ((memorySize >> 8) * 2)) & 3;
    std::cout<<i<<std::endl;

    return 0;
}