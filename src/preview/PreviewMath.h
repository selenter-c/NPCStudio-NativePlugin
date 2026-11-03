#pragma once
#include <cmath>
namespace NPCStudio::Preview {
inline float OrbitDelta(float previous, float next) { return -std::remainder(next - previous, 360.f); }
inline float ZoomDelta(float previous, float next) { return std::log(next / previous) / std::log(1.00150001049f); }
inline bool ValidPortrait(unsigned width, unsigned height) {
    return width == 144 && height == 144;
}
}
