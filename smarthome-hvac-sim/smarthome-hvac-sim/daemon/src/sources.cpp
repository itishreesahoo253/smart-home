#include "sources.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <chrono>

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "smarthome_ioctl.h"

namespace sh {

// ---------------- DeviceSource ----------------
DeviceSource::DeviceSource(const std::string& path) {
    fd_ = ::open(path.c_str(), O_RDWR | O_NONBLOCK);
    if (fd_ < 0)
        throw std::runtime_error("open(" + path + "): " + std::strerror(errno));
}

DeviceSource::~DeviceSource() { if (fd_ >= 0) ::close(fd_); }

bool DeviceSource::read(Reading& out, int timeout_ms) {
    pollfd p{fd_, POLLIN, 0};
    int r = ::poll(&p, 1, timeout_ms);
    if (r < 0) { if (errno == EINTR) return false; throw std::runtime_error("poll failed"); }
    if (r == 0) return false;

    sh_event ev{};
    ssize_t n = ::read(fd_, &ev, sizeof(ev));
    if (n < 0) { if (errno == EAGAIN || errno == EINTR) return false; throw std::runtime_error("read failed"); }
    if (n != (ssize_t)sizeof(ev)) return false;
    out.ts_ns = ev.ts_ns;
    out.temp_c = ev.temp_mc / 1000.0;
    out.motion = ev.motion != 0;
    return true;
}

void DeviceSource::setHvac(Hvac h) {
    int v = static_cast<int>(h);
    if (::ioctl(fd_, SH_IOC_SET_HVAC, &v) < 0) throw std::runtime_error("ioctl SET_HVAC failed");
}

void DeviceSource::setTarget(double c) {
    int v = static_cast<int>(c * 1000.0);
    if (::ioctl(fd_, SH_IOC_SET_TARGET, &v) < 0) throw std::runtime_error("ioctl SET_TARGET failed");
}

// ---------------- SimSource ----------------
SimSource::SimSource(int tick_ms, unsigned seed) : tick_ms_(tick_ms), rng_(seed ? seed : 1) {}

unsigned SimSource::next() {          // xorshift32
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return rng_;
}

bool SimSource::read(Reading& out, int) {
    std::this_thread::sleep_for(std::chrono::milliseconds(tick_ms_));
    unsigned r = next() % 100;
    if (occupied_ && r < 5) occupied_ = false;
    else if (!occupied_ && r < 4) occupied_ = true;
    bool motion = occupied_ && (next() % 100) < 70;

    double delta = (outdoor_ - temp_) / 50.0;
    Hvac h = hvac_.load();
    if (h == Hvac::Heat) delta += 0.3;
    else if (h == Hvac::Cool) delta -= 0.3;
    if (occupied_) delta += 0.02;
    temp_ += delta;

    now_ns_ += (uint64_t)tick_ms_ * 1000000ull;   // simulated time
    out.ts_ns = now_ns_;
    out.temp_c = temp_;
    out.motion = motion;
    return true;
}

} // namespace sh
