#include <cstdio>
#include "hvac_controller.hpp"
using namespace sh;

static int fails = 0, total = 0;
#define CHECK(c) do { ++total; if (!(c)) { ++fails; std::printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static Reading R(double s, double t, bool m) { return Reading{(uint64_t)(s * 1e9), t, m}; }

static void test_heat_cycle() {                    // TC-01
    HvacController c;
    CHECK(c.update(R(1, 22.0, true)) == Hvac::Heat);      // < 23.5
    CHECK(c.state() == CtrlState::Heating);
    CHECK(c.update(R(2, 23.8, true)) == Hvac::Heat);      // still < 24.0
    CHECK(c.update(R(3, 24.0, true)) == Hvac::Off);       // reached
    CHECK(c.state() == CtrlState::Idle);
}
static void test_cool_cycle() {                    // TC-02
    HvacController c;
    CHECK(c.update(R(1, 26.0, true)) == Hvac::Cool);
    CHECK(c.update(R(2, 24.2, true)) == Hvac::Cool);
    CHECK(c.update(R(3, 24.0, true)) == Hvac::Off);
}
static void test_no_chatter() {                    // TC-03 inside deadband => no change
    HvacController c;
    for (int i = 0; i < 10; ++i) CHECK(c.update(R(i, 23.7 + 0.05 * i, true)) == Hvac::Off);
}
static void test_eco_mode() {                      // TC-04
    HvacController c;                              // vacant from start: band 20..28
    CHECK(c.update(R(1, 26.0, false)) == Hvac::Off);      // would cool if occupied
    CHECK(!c.occupied());
    CHECK(c.update(R(2, 26.0, true)) == Hvac::Cool);      // motion => occupied
    CHECK(c.occupied());
}
static void test_vacancy_timeout() {               // TC-05
    HvacController c;
    c.update(R(0, 24.0, true));
    CHECK(c.occupied());
    c.update(R(29, 24.0, false)); CHECK(c.occupied());
    c.update(R(31, 24.0, false)); CHECK(!c.occupied());
}
static void test_fault() {                         // TC-06
    HvacController c;
    CHECK(c.update(R(1, 45.0, true)) == Hvac::Off);
    CHECK(c.state() == CtrlState::Fault);
    CHECK(c.update(R(2, 39.5, true)) == Hvac::Off);       // margin not met
    CHECK(c.state() == CtrlState::Fault);
    c.update(R(3, 30.0, true));
    CHECK(c.state() != CtrlState::Fault);
}
static void test_custom_target() {                 // TC-07
    Config cfg; cfg.target_c = 20.0;
    HvacController c(cfg);
    CHECK(c.update(R(1, 22.0, true)) == Hvac::Cool);
}

int main() {
    test_heat_cycle(); test_cool_cycle(); test_no_chatter();
    test_eco_mode(); test_vacancy_timeout(); test_fault(); test_custom_target();
    std::printf("%d checks, %d failed\n", total, fails);
    return fails ? 1 : 0;
}
