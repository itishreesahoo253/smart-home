// smarthome_daemon - occupancy-aware HVAC controller.
//   producer thread : ISource::read()  -> readings queue
//   main thread     : HvacController   -> ISource::setHvac(), log lines
//   logger thread   : stdout + CSV file
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#include "blocking_queue.hpp"
#include "hvac_controller.hpp"
#include "logger.hpp"
#include "sources.hpp"

using namespace sh;

static std::atomic<bool> g_stop{false};
static void onSignal(int) { g_stop = true; }

static void usage(const char* a) {
    std::printf("Usage: %s [--dev PATH | --sim] [--target C] [--vacancy-timeout S]\n"
                "          [--duration S] [--log FILE] [--tick MS]\n"
                "  --dev PATH   use kernel driver (default /dev/smarthome)\n"
                "  --sim        use built-in software simulator (no root/driver)\n", a);
}

int main(int argc, char** argv) {
    std::string dev = "/dev/smarthome", logfile;
    bool sim = false;
    int duration = 0, tick = 500;
    Config cfg;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto val = [&](const char* name) -> const char* {
            if (i + 1 >= argc) { std::fprintf(stderr, "missing value for %s\n", name); std::exit(2); }
            return argv[++i];
        };
        if (a == "--dev") dev = val("--dev");
        else if (a == "--sim") sim = true;
        else if (a == "--target") cfg.target_c = std::atof(val("--target"));
        else if (a == "--vacancy-timeout") cfg.vacancy_timeout_s = std::atof(val("--vacancy-timeout"));
        else if (a == "--duration") duration = std::atoi(val("--duration"));
        else if (a == "--log") logfile = val("--log");
        else if (a == "--tick") tick = std::atoi(val("--tick"));
        else { usage(argv[0]); return a == "--help" ? 0 : 2; }
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    std::unique_ptr<ISource> src;
    try {
        if (sim) src = std::make_unique<SimSource>(tick);
        else     src = std::make_unique<DeviceSource>(dev);
        src->setTarget(cfg.target_c);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n(hint: load the module, or run with --sim)\n", e.what());
        return 1;
    }

    Logger logger(logfile);
    BlockingQueue<Reading> readings;
    HvacController ctrl(cfg);
    logger.log("t_s,temp_c,motion,occupied,state,hvac,band_lo,band_hi");

    std::thread producer([&] {
        try {
            Reading r;
            while (!g_stop) if (src->read(r, 200)) readings.push(r);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "producer error: %s\n", e.what());
            g_stop = true;
        }
        readings.close();
    });

    const auto start = std::chrono::steady_clock::now();
    uint64_t t0 = 0; bool have_t0 = false;
    CtrlState prev = ctrl.state();
    Reading r;
    while (!g_stop) {
        if (duration > 0 && std::chrono::steady_clock::now() - start > std::chrono::seconds(duration)) break;
        if (!readings.pop(r, 200)) continue;
        if (!have_t0) { t0 = r.ts_ns; have_t0 = true; }

        Hvac cmd = ctrl.update(r);
        try { src->setHvac(cmd); } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); break; }

        char buf[160];
        std::snprintf(buf, sizeof buf, "%.1f,%.2f,%d,%d,%s,%s,%.1f,%.1f",
                      (r.ts_ns - t0) / 1e9, r.temp_c, r.motion, ctrl.occupied(),
                      toString(ctrl.state()), toString(cmd), ctrl.lowBand(), ctrl.highBand());
        logger.log(buf);
        if (ctrl.state() != prev) {
            logger.log(std::string("# EVENT: ") + toString(prev) + " -> " + toString(ctrl.state()));
            if (ctrl.state() == CtrlState::Fault) logger.log("# ALERT: temperature out of safe range, HVAC off");
            prev = ctrl.state();
        }
    }

    g_stop = true;
    producer.join();
    try { src->setHvac(Hvac::Off); } catch (...) {}
    logger.log("# daemon stopped, HVAC off");
    logger.stop();
    return 0;
}
