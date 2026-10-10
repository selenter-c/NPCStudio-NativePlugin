// TODO: Stage 8 — реализовать генерацию биографии через SkyrimNet LLM
// REF: см. SkyrimNetDevKit/CppAPI/PublicAPI.h — PublicSendCustomPromptToLLM (v8+)
#include "bio/BioGenerator.h"

namespace NPCStudio::Bio {

bool BioGenerator::GenerateBiography(const std::string& shortDescription, 
                                      const std::string& npcName,
                                      std::function<void(const std::string&)> callback) {
    // TODO: Stage 8 — вызов PublicSendCustomPromptToLLM с промптом для генерации био
    // TODO: Stage 8 — передача language: "Russian" в контекст
    // TODO: Stage 8 — асинхронный callback с результатом
    return false;
}

} // namespace NPCStudio::Bio