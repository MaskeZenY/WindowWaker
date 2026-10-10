#pragma once

#include "dolphin/types.h"
#include "dolphin/mtx.h"

#ifdef __cplusplus
extern "C" {
#endif

// Frame and tick lifecycle (Dusklight interp model)
void pc_interp_begin_frame(float step);
void pc_interp_begin_sim_tick(void);
void pc_interp_begin_record(void);
void pc_interp_end_record(void);
void pc_interp_begin_presentation(float step);
void pc_interp_end_presentation(void);

bool pc_interp_is_enabled(void);
bool pc_interp_is_presentation_active(void);
bool pc_interp_should_capture(void);
float pc_interp_get_step(void);

// Camera interpolation (Dusklight camera.cpp)
struct camera_process_class;
struct view_class;
void pc_interp_record_camera(struct camera_process_class* cam, int cameraId);
void pc_interp_interp_view(struct view_class* view);
bool pc_interp_camera_apply_presentation(void);
void pc_interp_camera_restore_presentation(void);
void pc_interp_camera_invalidate(void);

// Matrix replacement for models and actors (Dusklight frame_interpolation.cpp)
void pc_interp_record_final_mtx(const float m[3][4], const void* key);
bool pc_interp_lookup_replacement(const void* key, float out[3][4]);
bool pc_interp_lookup_concat_replacement(const void* lhs, const void* rhs, float out[3][4]);
void pc_interp_forget_mtx(const void* key);

#ifdef __cplusplus
}
#endif
