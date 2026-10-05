#pragma once
#include <atomic>
#include <string>
#include "common.hpp"

namespace sh {

// Abstraction over "where readings come from / where commands go".
class ISource {
public:
    virtual ~ISource() = default;
    virtual bool read(Reading& out, int timeout_ms) = 0;  // false on timeout
    virtual void setHvac(Hvac h) = 0;
    virtual void setTarget(double celsius) = 0;
};

// Real backend: /dev/smarthome (kernel driver). RAII file descriptor.
class DeviceSource : public ISource {
public:
    explicit DeviceSource(const std::string& path);
    ~DeviceSource() override;
    DeviceSource(const DeviceSource&) = delete;
    DeviceSource& operator=(const DeviceSource&) = delete;

    bool read(Reading& out, int timeout_ms) override;
    void setHvac(Hvac h) override;
    void setTarget(double celsius) override;
private:
    int fd_ = -1;
};

// Software backend with the same thermal model, for machines without the module.
class SimSource : public ISource {
public:
    explicit SimSource(int tick_ms = 500, unsigned seed = 42);
    bool read(Reading& out, int timeout_ms) override;
    void setHvac(Hvac h) override { hvac_ = h; }
    void setTarget(double) override {}
private:
    int tick_ms_;
    unsigned rng_;
    double temp_ = 24.0, outdoor_ = 32.0;
    bool occupied_ = false;
    std::atomic<Hvac> hvac_{Hvac::Off};   // written by control thread, read by producer
    uint64_t now_ns_ = 0;
    unsigned next();
};

} // namespace sh
