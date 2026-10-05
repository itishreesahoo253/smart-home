#pragma once
#include <atomic>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include "blocking_queue.hpp"

namespace sh {

// Asynchronous logger: producers push lines, one thread writes stdout + CSV file.
class Logger {
public:
    explicit Logger(const std::string& file) {
        if (!file.empty()) out_.open(file, std::ios::app);
        worker_ = std::thread([this] {
            std::string line;
            for (;;) {
                if (q_.pop(line, 100)) {
                    std::cout << line << '\n';
                    if (out_.is_open()) out_ << line << '\n';
                } else if (!running_) {
                    break;               // closed and fully drained
                }
            }
        });
    }
    ~Logger() { stop(); }
    void log(std::string line) { q_.push(std::move(line)); }
    void stop() {
        if (!worker_.joinable()) return;
        running_ = false;
        q_.close();
        worker_.join();
        std::cout.flush();
    }
private:
    BlockingQueue<std::string> q_;
    std::ofstream out_;
    std::thread worker_;
    std::atomic<bool> running_{true};
};

} // namespace sh
