#pragma once
#include <cstdint>
#include <cstddef>
namespace NPCStudio::Preview::Phantasm {
using Id = std::uint32_t;
using Result = std::uint32_t;
// Контракт восстановлен по Phantasm 0.3.0 ccf916f72d59. Другие сборки отклоняются.
struct Event {
    std::uint32_t size, kind;
    Id view, form;
    const char* reason;
    std::uint64_t reserved;
    const char* imageUrl;
};
struct Still {
    std::uint32_t size;
    Id request;
    bool ok;
    std::uint8_t reserved[7];
    const char* reason;
    std::uint32_t width, height;
    const std::uint8_t* pixels;
    std::uint32_t metadata, reserved2;
};
using EventFn = void (*)(const Event*, void*);
using StillFn = void (*)(const Still*, void*);
struct API {
    std::uint32_t size, version, providerVersion, reserved;
    Result (*RegisterClient)(const char*, EventFn, void*, Id*);
    Result (*ReleaseClient)(Id);
    Result (*OpenActor)(Id, const char*, Id, unsigned, unsigned, unsigned, Id*);
    Result (*CloseView)(Id);
    Result (*Resize)(Id, unsigned, unsigned);
    Result (*Orbit)(Id, float, float);
    Result (*Zoom)(Id, float, float, float);
    Result (*Refresh)(Id);
    Result (*SetHint)(Id, const char*, const char*);
    Result (*QueryHint)(Id, const char*);
    Result (*RequestStill)(Id, Id, unsigned, unsigned, unsigned, StillFn, void*, Id*);
    Result (*RebuildActor3D)(Id, const char*);
    Result (*OpenItem)(Id, const char*, Id, unsigned, unsigned, Id*);
    Result (*CancelStill)(Id, Id);
    Result (*RequestStillHinted)(Id, Id, unsigned, unsigned, unsigned, const char*, StillFn, void*, Id*);
    Result (*RequestItemStill)(Id, Id, unsigned, unsigned, const char*, StillFn, void*, Id*);
};
inline constexpr std::uint32_t FunctionRvas[] = {
    0x2a320, 0x2abe0, 0x2b4e0, 0x2b730, 0x2bb60, 0x2bc50, 0x2bcd0, 0x2e3f0,
    0x2e420, 0x2ef80, 0x2de30, 0x2f1b0, 0x2b610, 0x2e330, 0x2dfd0, 0x2e190
};
static_assert(sizeof(API) == 0x90 && offsetof(API, OpenItem) == 0x70);
static_assert(sizeof(Event) == 0x28 && offsetof(Event, imageUrl) == 0x20);
static_assert(sizeof(Still) == 0x30 && offsetof(Still, pixels) == 0x20 && offsetof(Still, metadata) == 0x28);
} // namespace NPCStudio::Preview::Phantasm
