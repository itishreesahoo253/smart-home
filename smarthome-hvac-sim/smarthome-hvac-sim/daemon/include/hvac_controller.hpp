#pragma once
#include "common.hpp"

namespace sh {

struct Config {
    double target_c          = 24.0;  // comfort target when occupied
    double hysteresis_c      = 0.5;   // occupied band = target +/- hysteresis
    double eco_offset_c      = 4.0;   // vacant band  = target +/- eco_offset
    double vacancy_timeout_s = 30.0;  // no motion this long => vacant
    double fault_low_c       = 5.0;
    double fault_high_c      = 40.0;
};

// Pure decision logic (no I/O) so it can be unit-tested without a device.
class HvacController {
public:
    explicit HvacController(Config c = Config{}) : cfg_(c) {}

    Hvac update(const Reading& r) {
        if (r.motion) { last_motion_ns_ = r.ts_ns; seen_motion_ = true; }
        const double timeout_ns = cfg_.vacancy_timeout_s * 1e9;
        occupied_ = seen_motion_ && (double)(r.ts_ns - last_motion_ns_) <= timeout_ns;

        const double t = r.temp_c;
        if (state_ == CtrlState::Fault) {                 // recover with 1 C margin
            if (t > cfg_.fault_low_c + 1.0 && t < cfg_.fault_high_c - 1.0) state_ = CtrlState::Idle;
            else return Hvac::Off;
        } else if (t < cfg_.fault_low_c || t > cfg_.fault_high_c) {
            state_ = CtrlState::Fault;
            return Hvac::Off;
        }

        const double band = occupied_ ? cfg_.hysteresis_c : cfg_.eco_offset_c;
        lo_ = cfg_.target_c - band;
        hi_ = cfg_.target_c + band;
        const double heat_off = lo_ + cfg_.hysteresis_c;  // stop heating
        const double cool_off = hi_ - cfg_.hysteresis_c;  // stop cooling

        switch (state_) {
            case CtrlState::Idle:
                if (t < lo_)      state_ = CtrlState::Heating;
                else if (t > hi_) state_ = CtrlState::Cooling;
                break;
            case CtrlState::Heating:
                if (t >= heat_off) state_ = CtrlState::Idle;
                break;
            case CtrlState::Cooling:
                if (t <= cool_off) state_ = CtrlState::Idle;
                break;
            case CtrlState::Fault: break;
        }
        return command();
    }

    Hvac command() const {
        switch (state_) {
            case CtrlState::Heating: return Hvac::Heat;
            case CtrlState::Cooling: return Hvac::Cool;
            default:                 return Hvac::Off;
        }
    }
    CtrlState state() const { return state_; }
    bool occupied() const { return occupied_; }
    double lowBand() const { return lo_; }
    double highBand() const { return hi_; }
    const Config& config() const { return cfg_; }

private:
    Config cfg_;
    CtrlState state_ = CtrlState::Idle;
    bool occupied_ = false, seen_motion_ = false;
    uint64_t last_motion_ns_ = 0;
    double lo_ = 0, hi_ = 0;
};

} // namespace sh
