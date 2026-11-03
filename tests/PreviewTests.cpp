#include "preview/PhantasmAPI.h"
#include "preview/PreviewMath.h"
#include <iostream>
#include <stdexcept>
using namespace NPCStudio::Preview;
void Require(bool value) { if (!value) throw std::runtime_error("Preview contract failure"); }
int main() {
    try {
        Require(OrbitDelta(179, -179) == -2 && OrbitDelta(-179, 179) == 2);
        Require(OrbitDelta(0, 90) == -90);
        for (float zoom : {.5f, 1.f, 1.5f, 2.5f}) {
            auto delta = ZoomDelta(1.f, zoom);
            Require(std::abs(std::pow(1.00150001049f, delta) - zoom) < .0001f);
            Require(std::abs(delta + ZoomDelta(zoom, 1.f)) < .001f);
        }
        Require(ValidPortrait(144, 144));
        Require(!ValidPortrait(0xffffffff, 144) && !ValidPortrait(144, 143));
        Require(!ValidPortrait(144, 0) && !ValidPortrait(0, 144));
        Require(Phantasm::FunctionRvas[12] == 0x2b610 && Phantasm::FunctionRvas[13] == 0x2e330);
        std::cout << "ABI layout, orbit wrapping, provider zoom math, portrait bounds passed; Skyrim runtime unverified\n";
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
