#include "heaptimer.h"

void HeapTimer::shiftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    size_t j = (i - 1) / 2;
    while (j >= 0) {
        if (heap_[j] < heap_[i]) break;
        SwapNode_(i, j);
        i = j;
        j = (i - 1)  / 2;
    }
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j; 
}

bool HeapTimer::shiftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while (j < n) {
        if (j + 1 < n && heap_[j + 1] < heap_[j]) j++;
        if (heap_[i] < heap_[j]) break;
        SwapNode_(i, j);
        i = j;
        j = 2 * i + 1;
    }
    return i > index;
}

void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    size_t i;
    if (ref_.count(id) == 0) {
        // 新节点，插入堆尾
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        shiftup_(i);
    } else {
        // 已有节点，调整节点
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeout);
        heap_[i].cb = cb;
        if (!shiftdown_(i, heap_.size())) {
            shiftup_(i);
        }
    }
}

void HeapTimer::doWork(int id) {
    // 删除指定结点，并触发回调函数
    if (heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    TimerNode node = heap_[i];
    node.cb();
    del_(i);
}

void HeapTimer::del_(size_t index) {
    // 删除指定节点
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    // 将要删除的节点换到堆尾，然后调整堆
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if (i < n) {
        SwapNode_(i, n);
        if (!shiftdown_(i, n)) {
            shiftup_(i);
        }
    }
    // 删除堆尾元素
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

void HeapTimer::adjust(int id, int newExpires) {
    // 调整指定节点
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(newExpires);
    shiftdown_(ref_[id], heap_.size());
}

void HeapTimer::tick() {
    // 清除超时节点
    if (heap_.empty()) {
        return;
    }
    while (!heap_.empty()) {
        TimerNode node = heap_.front();
        if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

int HeapTimer::GetNextTick() {
    tick();
    size_t res = -1;
    if (!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if (res < 0) res = 0;
    }
    return res;
}