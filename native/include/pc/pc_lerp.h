#pragma once

#include "SSystem/SComponent/c_angle.h"
#include "SSystem/SComponent/c_xyz.h"
#include "dolphin/mtx.h"

#include <algorithm>
#include <cmath>

namespace pc::interp {

inline s16 lerp_angle(s16 lhs, s16 rhs, float step) {
    const f32 ra = S2RAD(lhs);
    const f32 d = remainderf(S2RAD(rhs) - ra, 2.0f * 3.14159265358979323846f);
    return cAngle::Radian_to_SAngle(ra + d * step);
}

inline f32 lerp(f32 lhs, f32 rhs, float step) {
    return lhs + (rhs - lhs) * step;
}

inline void lerp(f32& out, const f32& lhs, const f32& rhs, float step) {
    out = lerp(lhs, rhs, step);
}

inline void lerp(cXyz& out, const cXyz& lhs, const cXyz& rhs, float step) {
    out.x = lerp(lhs.x, rhs.x, step);
    out.y = lerp(lhs.y, rhs.y, step);
    out.z = lerp(lhs.z, rhs.z, step);
}

inline void lerp(Mtx& out, const Mtx& lhs, const Mtx& rhs, float step) {
    for (size_t row = 0; row < 3; ++row) {
        for (size_t col = 0; col < 4; ++col) {
            const float l = lhs[row][col];
            out[row][col] = l + (rhs[row][col] - l) * step;
        }
    }
}

} // namespace pc::interp
