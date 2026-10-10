#pragma once
// TODO: Stage 8 — реализовать генерацию биографии через SkyrimNet LLM
// REF: см. SkyrimNetDevKit/CppAPI/PublicAPI.h — PublicSendCustomPromptToLLM (v8+)
#include <string>
#include <functional>

namespace NPCStudio::Bio {
    class BioGenerator {
    public:
        static bool GenerateBiography(const std::string& shortDescription, 
                                      const std::string& npcName,
                                      std::function<void(const std::string&)> callback);
    };
}