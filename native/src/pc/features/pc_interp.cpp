#include "pc/pc_interp.h"
#include "pc/pc_clock.h"
#include "pc/pc_fps.h"
#include "pc/pc_lerp.h"

#include "dolphin/mtx.h"
#include "f_op/f_op_camera_mng.h"
#include "f_op/f_op_view.h"
#include "d/d_com_inf_game.h"
#include "d/d_stage.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_lib.h"
#include "JSystem/J3DGraphBase/J3DSys.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace {

struct CameraSnapshot {
    cXyz eye{};
    cXyz center{};
    cXyz up{0.0f, 1.0f, 0.0f};
    s16 bank = 0;
    f32 fovy = 60.0f;
    f32 aspect = 1.333333f;
    f32 near_ = 30.0f;
    f32 far_ = 100000.0f;
    bool valid = false;
};

static CameraSnapshot s_camPrev{};
static CameraSnapshot s_camCurr{};
static view_class s_presentationViewBackup{};
static bool s_cameraPresentationActive = false;

struct Matrix34 {
    float m[3][4];
};

static bool s_recording = false;
static bool s_replacementsActive = false;
static int s_presentationDepth = 0;
static float s_step = 0.0f;

static std::unordered_map<uintptr_t, Matrix34> s_currentRecording;
static std::unordered_map<uintptr_t, Matrix34> s_previousRecording;
static std::unordered_map<uintptr_t, Matrix34> g_replacements;

static void copy_view_to_snap(CameraSnapshot* dst, const view_class& v) {
    dst->eye = v.mLookat.mEye;
    dst->center = v.mLookat.mCenter;
    dst->up = v.mLookat.mUp;
    dst->bank = v.mBank;
    dst->fovy = v.mFovy;
    dst->aspect = v.mAspect;
    dst->near_ = v.mNear;
    dst->far_ = v.mFar;
    dst->valid = true;
}

static void apply_presented_view(view_class* view) {
    C_MTXPerspective(view->mProjMtx, view->mFovy, view->mAspect, view->mNear, view->mFar);
    mDoMtx_lookAt(view->mViewMtx, &view->mLookat.mEye, &view->mLookat.mCenter, &view->mLookat.mUp, view->mBank);

    j3dSys.setViewMtx(view->mViewMtx);
    cMtx_inverse(view->mViewMtx, view->mInvViewMtx);

    MTXCopy(view->mViewMtx, view->mViewMtxNoTrans);
    view->mViewMtxNoTrans[0][3] = 0.0f;
    view->mViewMtxNoTrans[1][3] = 0.0f;
    view->mViewMtxNoTrans[2][3] = 0.0f;
    cMtx_concatProjView(view->mProjMtx, view->mViewMtx, view->mProjViewMtx);

    f32 farPlane;
    if (dComIfGp_getScopeMesgStatus() != fopMsgStts_MSG_UNK0_e) {
        farPlane = view->mFar;
    } else {
        stage_stag_info_class* stag = dComIfGp_getStageStagInfo();
        farPlane = stag ? dStage_stagInfo_GetCullPoint(stag) : view->mFar;
    }

    mDoLib_clipper::setup(view->mFovy, view->mAspect, view->mNear, farPlane);
}

} // namespace

extern "C" {

void pc_interp_begin_frame(float step) {
    s_step = std::clamp(step, 0.0f, 1.0f);
    if (!pc_interp_is_enabled()) {
        s_recording = false;
        s_replacementsActive = false;
        s_previousRecording.clear();
        s_currentRecording.clear();
        g_replacements.clear();
        s_camPrev.valid = false;
        s_camCurr.valid = false;
        s_presentationDepth = 0;
        s_cameraPresentationActive = false;
    }
}

void pc_interp_begin_sim_tick(void) {
    if (!pc_interp_is_enabled()) {
        return;
    }
    if (s_camCurr.valid) {
        s_camPrev = s_camCurr;
    }
    s_previousRecording = std::move(s_currentRecording);
    s_currentRecording.clear();
    g_replacements.clear();
    s_replacementsActive = false;
}

void pc_interp_begin_record(void) {
    if (!pc_interp_is_enabled()) {
        s_recording = false;
        return;
    }
    s_recording = true;
}

void pc_interp_end_record(void) {
    s_recording = false;
}

void pc_interp_begin_presentation(float step) {
    s_step = std::clamp(step, 0.0f, 1.0f);
    if (!pc_interp_is_enabled()) {
        return;
    }

    // Interpolate model matrices
    g_replacements.clear();
    s_replacementsActive = !s_recording && !s_previousRecording.empty() && !s_currentRecording.empty();
    if (s_replacementsActive) {
        for (const auto& kv : s_previousRecording) {
            auto it = s_currentRecording.find(kv.first);
            if (it != s_currentRecording.end()) {
                Matrix34& out = g_replacements[kv.first];
                const Matrix34& oldMtx = kv.second;
                const Matrix34& newMtx = it->second;
                for (int r = 0; r < 3; ++r) {
                    for (int c = 0; c < 4; ++c) {
                        float o = oldMtx.m[r][c];
                        float n = newMtx.m[r][c];
                        out.m[r][c] = o + (n - o) * s_step;
                    }
                }
            }
        }
    }

    if (s_presentationDepth > 0) {
        s_presentationDepth++;
        return;
    }
    s_presentationDepth = 1;
    s_cameraPresentationActive = pc_interp_camera_apply_presentation();
}

void pc_interp_end_presentation(void) {
    if (s_presentationDepth == 0) {
        return;
    }
    s_presentationDepth--;
    if (s_presentationDepth > 0) {
        return;
    }

    if (s_cameraPresentationActive) {
        pc_interp_camera_restore_presentation();
        s_cameraPresentationActive = false;
    }
}

bool pc_interp_is_enabled(void) {
    return pc_target_fps() > 30;
}

bool pc_interp_is_presentation_active(void) {
    return s_presentationDepth > 0;
}

bool pc_interp_should_capture(void) {
    return pc_interp_is_enabled() && s_recording;
}

float pc_interp_get_step(void) {
    return s_step;
}

void pc_interp_record_camera(camera_process_class* cam, int cameraId) {
    if (!pc_interp_is_enabled() || cameraId != 0 || cam == nullptr) {
        return;
    }
    copy_view_to_snap(&s_camCurr, cam->view);
    if (!s_camPrev.valid) {
        s_camPrev = s_camCurr;
    }
}

void pc_interp_interp_view(view_class* view) {
    if (!pc_interp_is_enabled() || !view) {
        return;
    }
    if (!s_camPrev.valid || !s_camCurr.valid) {
        return;
    }

    const float step = s_step;
    const bool authoritative = (step >= 1.0f);

    cXyz eye;
    cXyz center;
    cXyz up;
    if (authoritative) {
        eye = s_camCurr.eye;
        center = s_camCurr.center;
        up = s_camCurr.up;
    } else {
        pc::interp::lerp(eye, s_camPrev.eye, s_camCurr.eye, step);
        pc::interp::lerp(center, s_camPrev.center, s_camCurr.center, step);
        pc::interp::lerp(up, s_camPrev.up, s_camCurr.up, step);
    }
    if (!up.normalizeRS()) {
        up = s_camCurr.up;
        up.normalizeRS();
    }

    view->mLookat.mEye = eye;
    view->mLookat.mCenter = center;
    view->mLookat.mUp = up;
    if (authoritative) {
        view->mBank = s_camCurr.bank;
        view->mFovy = s_camCurr.fovy;
        view->mAspect = s_camCurr.aspect;
        view->mNear = s_camCurr.near_;
        view->mFar = s_camCurr.far_;
    } else {
        view->mBank = pc::interp::lerp_angle(s_camPrev.bank, s_camCurr.bank, step);
        view->mFovy = s_camPrev.fovy + (s_camCurr.fovy - s_camPrev.fovy) * step;
        view->mAspect = s_camPrev.aspect + (s_camCurr.aspect - s_camPrev.aspect) * step;
        view->mNear = s_camPrev.near_ + (s_camCurr.near_ - s_camPrev.near_) * step;
        view->mFar = s_camPrev.far_ + (s_camCurr.far_ - s_camPrev.far_) * step;
    }
}

bool pc_interp_camera_apply_presentation(void) {
    if (!s_camPrev.valid || !s_camCurr.valid) {
        return false;
    }

    view_class* const view = dComIfGd_getView();
    if (view == nullptr) {
        return false;
    }

    std::memcpy(static_cast<void*>(&s_presentationViewBackup), static_cast<const void*>(view), sizeof(view_class));
    pc_interp_interp_view(view);
    apply_presented_view(view);
    return true;
}

void pc_interp_camera_restore_presentation(void) {
    view_class* const view = dComIfGd_getView();
    if (view != nullptr) {
        std::memcpy(static_cast<void*>(view), static_cast<const void*>(&s_presentationViewBackup), sizeof(view_class));
    }
}

void pc_interp_camera_invalidate(void) {
    s_camPrev.valid = false;
    s_camCurr.valid = false;
}

void pc_interp_record_final_mtx(const float m[3][4], const void* key) {
    if (!s_recording || m == nullptr || key == nullptr) {
        return;
    }
    Matrix34& dst = s_currentRecording[reinterpret_cast<uintptr_t>(key)];
    std::memcpy(dst.m, m, sizeof(dst.m));
}

bool pc_interp_lookup_replacement(const void* key, float out[3][4]) {
    if (!s_replacementsActive || key == nullptr || out == nullptr) {
        return false;
    }
    auto it = g_replacements.find(reinterpret_cast<uintptr_t>(key));
    if (it == g_replacements.end()) {
        return false;
    }
    std::memcpy(out, it->second.m, sizeof(float) * 12);
    return true;
}

static const float (*resolve_replacement(const float (*source)[4], float scratch[3][4]))[4] {
    if (!s_replacementsActive || source == nullptr) {
        return source;
    }
    auto it = g_replacements.find(reinterpret_cast<uintptr_t>(source));
    if (it == g_replacements.end()) {
        return source;
    }
    std::memcpy(scratch, it->second.m, sizeof(float) * 12);
    return scratch;
}

bool pc_interp_lookup_concat_replacement(const void* lhs, const void* rhs, float out[3][4]) {
    if (!s_replacementsActive || lhs == nullptr || rhs == nullptr || out == nullptr) {
        return false;
    }
    float lhs_scratch[3][4];
    float rhs_scratch[3][4];
    const float (*resolved_lhs)[4] = resolve_replacement(reinterpret_cast<const float (*)[4]>(lhs), lhs_scratch);
    const float (*resolved_rhs)[4] = resolve_replacement(reinterpret_cast<const float (*)[4]>(rhs), rhs_scratch);
    if (resolved_lhs == reinterpret_cast<const float (*)[4]>(lhs) &&
        resolved_rhs == reinterpret_cast<const float (*)[4]>(rhs))
    {
        return false;
    }
    MTXConcat(const_cast<float (*)[4]>(resolved_lhs), const_cast<float (*)[4]>(resolved_rhs), out);
    return true;
}

void pc_interp_forget_mtx(const void* key) {
    const auto addr = reinterpret_cast<uintptr_t>(key);
    s_previousRecording.erase(addr);
    s_currentRecording.erase(addr);
    g_replacements.erase(addr);
}

} // extern "C"
