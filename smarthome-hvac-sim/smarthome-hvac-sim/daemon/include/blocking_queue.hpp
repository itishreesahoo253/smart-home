#pragma once
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace sh {

// Thread-safe FIFO with timeout pop and close() for clean shutdown.
template <typename T>
class BlockingQueue {
public:
    void push(T v) {
        { std::lock_guard<std::mutex> lk(m_); if (closed_) return; q_.push_back(std::move(v)); }
        cv_.notify_one();
    }
    // false => timeout, or closed and drained.
    bool pop(T& out, int timeout_ms) {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms), [&] { return !q_.empty() || closed_; });
        if (q_.empty()) return false;
        out = std::move(q_.front());
        q_.pop_front();
        return true;
    }
    void close() {
        { std::lock_guard<std::mutex> lk(m_); closed_ = true; }
        cv_.notify_all();
    }
private:
    std::mutex m_;
    std::condition_variable cv_;
    std::deque<T> q_;
    bool closed_ = false;
};

} // namespace sh
