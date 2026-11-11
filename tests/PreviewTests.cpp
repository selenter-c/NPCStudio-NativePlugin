#include "preview/PhantasmProvider.h"
#include "preview/PreviewMath.h"
#include <iostream>
#include <stdexcept>
using namespace NPCStudio::Preview;
void Require(bool value) { if (!value) throw std::runtime_error("Preview contract failure"); }
int main() {
    try {
        PHANTASM_API::Interface1 api{};
        api.size = sizeof(api);
        api.interfaceVersion = PHANTASM_API::kInterface1;
        api.pluginVersion = 300;
        Require(!SupportsPhantasm(nullptr) && !SupportsPhantasm(&api));
        api.pluginVersion = 10000;
        Require(SupportsPhantasm(&api));
        api.size += 32;
        api.pluginVersion = 10100;
        Require(SupportsPhantasm(&api));
        api.size = offsetof(PHANTASM_API::Interface1, RequestItemStill);
        Require(!SupportsPhantasm(&api));
        Require(OrbitDelta(179, -179) == -2 && OrbitDelta(-179, 179) == 2);
        Require(OrbitDelta(0, 90) == -90);
        for (float zoom : {.5f, 1.f, 1.5f, 2.5f}) {
            auto delta = ZoomDelta(1.f, zoom);
            Require(std::abs(std::pow(1.00150001049f, delta) - zoom) < .0001f);
            Require(std::abs(delta + ZoomDelta(zoom, 1.f)) < .001f);
        }
        Require(std::abs(CameraScale(.25f) - 2.6f) < .00001f);
        Require(std::abs(CameraScale(1.f) - .8375f) < .00001f);
        Require(std::abs(CameraScale(4.f) - .45f) < .00001f);
        Require(ValidPortrait(144, 144));
        Require(!ValidPortrait(0xffffffff, 144) && !ValidPortrait(144, 143));
        Require(!ValidPortrait(144, 0) && !ValidPortrait(0, 144));
        std::cout << "ABI layout, orbit wrapping, provider zoom math, portrait bounds passed; Skyrim runtime unverified\n";
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
