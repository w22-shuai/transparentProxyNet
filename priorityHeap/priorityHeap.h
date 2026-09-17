#pragma once
#include <vector>
#include <stdint.h>

/*单线程小顶堆设计,应当有以下几个必要函数,插入
 *
 */
template<typename T>
class priorityHeap {
private:
    std::vector<T> list_;



public:
    priorityHeap()=default;
    bool empty() {
        return list_.empty();
    }
    T& front() {
        return list_[0];
    }
    void push(T&&) {

    }
    void update(uint32_t index,T&& node) {

    }
    void remove(uint32_t index) {

    }
};