#include "pc/pc_clock.h"
#include "pc/pc_fps.h"

#include <algorithm>
#include <chrono>
#include <ctime>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

namespace {

static uint64_t monotonicNs(void) {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

static void preciseSleepNs(uint64_t durationNs) {
    if (durationNs == 0) return;
#if defined(_WIN32)
    // On Windows, if duration > 2ms use Sleep, then spin on high precision clock
    const uint64_t target = monotonicNs() + durationNs;
    if (durationNs > 2000000ull) {
        Sleep((DWORD)((durationNs - 2000000ull) / 1000000ull));
    }
    while (monotonicNs() < target) {
        YieldProcessor();
    }
#else
    const uint64_t target = monotonicNs() + durationNs;
    if (durationNs > 2000000ull) {
        const uint64_t ns = durationNs - 2000000ull;
        struct timespec ts = {(time_t)(ns / 1000000000ull), (long)(ns % 1000000000ull)};
        nanosleep(&ts, nullptr);
    }
    while (monotonicNs() < target) {
    }
#endif
}

constexpr uint64_t kSimPeriodNs = 33333333ull; // 30 Hz = 33.33ms

static bool s_initialized = false;
static uint64_t s_previousNativeSampleNs = 0;
static uint64_t s_latestGameSampleNs = 0;
static uint64_t s_currentSnapshotNs = 0;
static uint64_t s_pendingSimTimeNs = 0;
static bool s_simTickActive = false;
static bool s_separatePresentation = false;
static float s_lastInterpStep = 1.0f;

} // namespace

extern "C" {

void pc_clock_init(void) {
    const uint64_t now = monotonicNs();
    s_previousNativeSampleNs = now;
    s_latestGameSampleNs = now;
    s_currentSnapshotNs = (now > kSimPeriodNs) ? (now - kSimPeriodNs) : 0;
    s_pendingSimTimeNs = s_currentSnapshotNs;
    s_simTickActive = false;
    s_initialized = true;
}

PcFrameTiming pc_clock_advance(void) {
    if (!s_initialized) {
        pc_clock_init();
    }

    const uint64_t nativeNow = monotonicNs();
    const uint64_t gameNow = nativeNow;
    const uint64_t nativeFrameGap = nativeNow - s_previousNativeSampleNs;
    s_previousNativeSampleNs = nativeNow;
    s_latestGameSampleNs = gameNow;

    const int targetFps = pc_target_fps();
    const bool interpolating = (targetFps > 30);
    const bool separate = interpolating;
    s_separatePresentation = separate;

    PcFrameTiming out;
    out.dt = (float)nativeFrameGap / 1000000000.0f;
    out.separatePresentation = separate;

    if (!separate) {
        // Mode 1: 30 FPS console native
        s_currentSnapshotNs = gameNow;
        out.numSimTicks = 1;
        out.interpStep = 1.0f;
        s_lastInterpStep = 1.0f;
        return out;
    }

    // Mode 2: Decoupled presentation (Dusklight model)
    // 1 tick latency when interpolating, so presentation renders between snapshot (now - dt) and curr (now)
    const uint64_t simulationTarget = interpolating ?
        ((gameNow > kSimPeriodNs) ? (gameNow - kSimPeriodNs) : 0) : gameNow;

    const bool abnormalGap = (nativeFrameGap > 250000000ull); // > 250ms
    if (abnormalGap) {
        s_currentSnapshotNs = simulationTarget;
        out.numSimTicks = 0;
        out.interpStep = 1.0f;
        s_lastInterpStep = 1.0f;
        return out;
    }

    int numSimTicks = 0;
    uint64_t projectedSnapshotTime = s_currentSnapshotNs;
    constexpr int kMaxSimTicksPerFrame = 4;
    while (numSimTicks < kMaxSimTicksPerFrame) {
        const bool tickDue = interpolating ?
            (projectedSnapshotTime < simulationTarget) :
            (projectedSnapshotTime + kSimPeriodNs <= simulationTarget);
        if (!tickDue) {
            break;
        }
        projectedSnapshotTime += kSimPeriodNs;
        numSimTicks++;
    }

    out.numSimTicks = numSimTicks;
    const float step = pc_clock_sample_interp_step();
    out.interpStep = step;
    s_lastInterpStep = step;
    return out;
}

void pc_clock_begin_sim_tick(void) {
    s_pendingSimTimeNs = s_separatePresentation ? (s_currentSnapshotNs + kSimPeriodNs) : s_latestGameSampleNs;
    s_simTickActive = true;
}

void pc_clock_commit_sim_tick(void) {
    if (s_simTickActive) {
        s_currentSnapshotNs = s_pendingSimTimeNs;
        s_simTickActive = false;
    } else {
        s_currentSnapshotNs += kSimPeriodNs;
    }
}

bool pc_clock_is_separate_presentation(void) {
    return s_separatePresentation;
}

float pc_clock_sample_interp_step(void) {
    const uint64_t now = monotonicNs();
    if (now <= s_currentSnapshotNs) {
        return 0.0f;
    }
    const float step = (float)(now - s_currentSnapshotNs) / (float)kSimPeriodNs;
    return std::clamp(step, 0.0f, 1.0f);
}

void pc_clock_frame_limit(void) {
    static uint64_t s_lastLimitNs = 0;
    const int targetFps = pc_target_fps();
    if (targetFps <= 0) return;

    const uint64_t targetPeriodNs = 1000000000ull / (uint64_t)targetFps;
    const uint64_t now = monotonicNs();

    if (s_lastLimitNs != 0) {
        const uint64_t elapsed = now - s_lastLimitNs;
        if (elapsed < targetPeriodNs) {
            preciseSleepNs(targetPeriodNs - elapsed);
        }
    }
    s_lastLimitNs = monotonicNs();
}

}
