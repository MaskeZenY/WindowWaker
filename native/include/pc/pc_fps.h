#ifndef PC_FPS_H
#define PC_FPS_H

#ifdef __cplusplus
extern "C" {
#endif

int pc_target_fps(void);
float pc_time_step(void);
void pc_target_fps_set(int fps);

#ifdef __cplusplus
}
#endif

#endif // PC_FPS_H
