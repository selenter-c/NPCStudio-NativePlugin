#pragma once
#include <cmath>
namespace NPCStudio::Preview {
inline float OrbitDelta(float previous, float next) { return -std::remainder(next - previous, 360.f); }
inline float ZoomDelta(float previous, float next) { return std::log(next / previous) / std::log(1.00150001049f); }
// Границы Zoom провайдера Phantasm 0.3.0, проверены по его clamp.
inline float CameraScale(float zoom) {
    return zoom >= 1.f ? .8375f * std::pow(.45f / .8375f, (zoom - 1.f) / 3.f)
                      : .8375f * std::pow(2.6f / .8375f, (1.f - zoom) / .75f);
}
inline bool ValidPortrait(unsigned width, unsigned height) {
    return width == 144 && height == 144;
}
}
