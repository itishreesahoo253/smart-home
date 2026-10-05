#pragma once
#include <cstdint>

namespace sh {

enum class Hvac : int { Off = 0, Heat = 1, Cool = 2 };
enum class CtrlState { Idle, Heating, Cooling, Fault };

struct Reading {
    uint64_t ts_ns = 0;
    double   temp_c = 0.0;
    bool     motion = false;
};

inline const char* toString(Hvac h) {
    switch (h) { case Hvac::Heat: return "HEAT"; case Hvac::Cool: return "COOL"; default: return "OFF"; }
}
inline const char* toString(CtrlState s) {
    switch (s) {
        case CtrlState::Heating: return "HEATING";
        case CtrlState::Cooling: return "COOLING";
        case CtrlState::Fault:   return "FAULT";
        default:                 return "IDLE";
    }
}

} // namespace sh
