#pragma once
#include "Phantasm_API.h"
#include <cstddef>

namespace NPCStudio::Preview {
inline bool SupportsPhantasm(const PHANTASM_API::Interface1* api) {
    return api &&
        api->size >= offsetof(PHANTASM_API::Interface1, RequestItemStill) + sizeof(api->RequestItemStill) &&
        api->interfaceVersion == PHANTASM_API::kInterface1 && api->pluginVersion >= 10000;
}
}
