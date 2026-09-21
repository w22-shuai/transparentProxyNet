#pragma once
#include <vector>
#include <cstdint>
#include <cassert>
#include <utility>

/*单线程小顶堆设计,数组(vector)实现的二叉堆。
 *
 * T 需要满足的隐式约定(不是强制concept,但少了任一项都无法通过编译/无法正确工作):
 *   1. bool operator<(const T&) const   —— 决定堆序,小的排在上面
 *   2. void setHeapIndex(uint32_t idx)  —— 把"当前在数组里的下标"回写给T(或T持有的对象)
 *   3. uint32_t getHeapIndex() const    —— 读回上面回写的下标
 *
 * 2/3之所以必须,是因为update()/remove()都是"调用方拿着一个下标直接定位",
 * 这个下标必须始终和元素在list_里的真实位置保持一致——每次swap都要把
 * 双方的下标重新回写,否则调用方拿着过期下标操作就是未定义行为。
 *
 * 非线程安全:所有操作必须在同一线程内串行调用。
 */
template<typename T>
class priorityHeap {
private:
    std::vector<T> list_;

    static uint32_t parentOf(uint32_t idx) { return (idx - 1) / 2; }
    static uint32_t leftOf(uint32_t idx)   { return idx * 2 + 1; }
    static uint32_t rightOf(uint32_t idx)  { return idx * 2 + 2; }

    void swapNode(uint32_t i, uint32_t j) {
        std::swap(list_[i], list_[j]);
        list_[i].setHeapIndex(i);
        list_[j].setHeapIndex(j);
    }

    //把idx处元素往上浮,返回它最终停留的下标
    uint32_t siftUp(uint32_t idx) {
        while (idx > 0) {
            uint32_t parent = parentOf(idx);
            if (list_[idx] < list_[parent]) {
                swapNode(idx, parent);
                idx = parent;
            } else {
                break;
            }
        }
        return idx;
    }

    //把idx处元素往下沉,返回它最终停留的下标
    uint32_t siftDown(uint32_t idx) {
        uint32_t n = static_cast<uint32_t>(list_.size());
        for (;;) {
            uint32_t left = leftOf(idx);
            uint32_t right = rightOf(idx);
            uint32_t smallest = idx;
            if (left < n && list_[left] < list_[smallest]) {
                smallest = left;
            }
            if (right < n && list_[right] < list_[smallest]) {
                smallest = right;
            }
            if (smallest == idx) {
                break;
            }
            swapNode(idx, smallest);
            idx = smallest;
        }
        return idx;
    }

public:
    priorityHeap() = default;
    uint32_t size() const {
        return static_cast<uint32_t>(list_.size());
    }

    bool empty() const {
        return list_.empty();
    }

    T& top() {
        return list_[0];
    }

    void push(T&& node) {
        list_.push_back(std::move(node));
        uint32_t idx = size() - 1;
        list_[idx].setHeapIndex(idx);
        siftUp(idx);
    }

    //原地替换index处元素并重新调整堆序;新key可能变大也可能变小,两个方向都要试
    void update(uint32_t index, T&& node) {
        list_[index] = std::move(node);
        list_[index].setHeapIndex(index);
        uint32_t newIdx = siftUp(index);
        if (newIdx == index) {   //没能往上浮,说明可能需要往下沉
            siftDown(index);
        }
    }

    void remove(uint32_t index) {
        uint32_t lastIdx = size() - 1;
        if (index == lastIdx) {
            list_.pop_back();
            return;
        }
        swapNode(index, lastIdx);
        list_.pop_back();
        uint32_t newIdx = siftUp(index);
        if (newIdx == index) {
            siftDown(index);
        }
    }
};