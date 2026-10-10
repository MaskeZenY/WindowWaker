#pragma once

#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PcFrameTiming {
    float dt;
    bool separatePresentation;
    int numSimTicks;
    float interpStep;
} PcFrameTiming;

void pc_clock_init(void);
PcFrameTiming pc_clock_advance(void);
void pc_clock_begin_sim_tick(void);
void pc_clock_commit_sim_tick(void);
bool pc_clock_is_separate_presentation(void);
float pc_clock_sample_interp_step(void);
void pc_clock_frame_limit(void);

#include "pc/pc_interp.h"

#ifdef __cplusplus
}
#endif
