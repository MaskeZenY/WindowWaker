// COS_TARGET_FPS: 30 (console native), 60 (fluid native PC default), 120 (high refresh).
#include "pc/pc_fps.h"
#include "pc/pc_settings.h"

#include <cstdlib>
#include <cstring>

namespace {

int sTargetFps = -1;

int parse() {
    const char* v = getenv("COS_TARGET_FPS");
    if (v == nullptr || v[0] == '\0') {
        v = pc_settings_get("COS_TARGET_FPS");
    }
    if (v == nullptr || v[0] == '\0') {
        return 60; // 60 FPS default with decoupled architecture
    }
    int fps = atoi(v);
    if (fps <= 0) return 60;
    return fps;
}

} // namespace

extern "C" {

int pc_target_fps(void) {
    if (sTargetFps <= 0) {
        sTargetFps = parse();
    }
    return sTargetFps;
}

float pc_time_step(void) {
    int fps = pc_target_fps();
    if (fps <= 0) return 1.0f;
    return 30.0f / (float)fps;
}

void pc_target_fps_set(int fps) {
    if (fps <= 0) fps = 60;
    sTargetFps = fps;
}

}
