#include "preview/ActorPreview.h"
#include "preview/PhantasmProvider.h"
#include "preview/PreviewMath.h"
#include "ui/MagelightBridge.h"
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <cmath>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#pragma warning(push)
#pragma warning(disable : 4244 4267 4996)
#include "thirdparty/stb_image_write.h"
#pragma warning(pop)

namespace NPCStudio::Preview {
using json = nlohmann::json;
namespace P = PHANTASM_API;
static const P::Interface1* Api = nullptr;
static P::ClientId Client = 0;
static std::mutex Guard;
static std::uint64_t Generation = 1;
struct LiveState {
    P::ViewId view = 0;
    RE::FormID form = 0;
    std::string identity, image, kind;
    float yaw = 0, pitch = 0, zoom = 1;
    float panX = 0, panY = 0;
    std::string lighting, worn;
    bool motion = false, waiting = false, lastSeen = false;
};
static LiveState Live;
struct PortraitRequest {
    std::string identity;
    std::uint64_t generation;
    float x = 0, y = 0, size = 1;
    bool crop = false;
    unsigned thumbnailToken = 0;
    std::uint32_t stillId = 0;
    std::filesystem::path thumbnailCache;
};
static std::unordered_map<std::uintptr_t, PortraitRequest> Portraits;
static std::uintptr_t NextPortrait = 0;
static std::filesystem::path PortraitPath(const std::string& identity) {
    if (identity.empty() || identity.size() > 120 || identity.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") != std::string::npos)
        throw std::runtime_error("Invalid portrait identity");
    return std::filesystem::path("Data/SKSE/Plugins/NPCStudio/Portraits") / (identity + ".image");
}
static void StoreImage(const std::filesystem::path& path, const std::string& image, DWORD flags = MOVEFILE_REPLACE_EXISTING) {
    std::filesystem::create_directories(path.parent_path());
    const auto temp = std::filesystem::path(path.wstring() + L".tmp");
    { std::ofstream file(temp, std::ios::binary); file << image; file.flush(); if (!file) throw std::runtime_error("Cannot save portrait"); }
    if (!MoveFileExW(temp.c_str(), path.c_str(), flags)) throw std::runtime_error("Cannot replace image");
}
void SavePortrait(const std::string& identity, const std::string& image) {
    if (!image.starts_with("data:image/png;base64,") || image.size() > 2000000) throw std::runtime_error("Invalid portrait image");
    StoreImage(PortraitPath(identity), image, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}
static std::string ReadImage(const std::filesystem::path& path, std::uintmax_t maxSize) {
    std::error_code error;
    if (std::filesystem::file_size(path, error) >= maxSize || error) return {};
    std::ifstream file(path, std::ios::binary);
    std::string image((std::istreambuf_iterator<char>(file)), {});
    return image.starts_with("data:image/png;base64,") ? image : std::string{};
}
static json Describe(const LiveState& state) {
    return {{"identity", state.identity}, {"kind", state.kind}, {"image", state.image},
        {"pending", state.image.empty()}, {"waiting", state.waiting}, {"lastSeen", state.lastSeen}, {"backend", "Phantasm"}};
}
static void Check(P::Result result, const char* operation) {
    if (result != P::Result::Ok) throw std::runtime_error(fmt::format("Phantasm {}: result {}", operation, static_cast<int>(result)));
}
static void MoveCamera(P::ViewId view, const LiveState& previous, const LiveState& next, bool fresh) {
    const auto yaw = next.yaw - (!next.worn.empty() && next.worn != "off" ? 180.f : 0.f);
    const auto oldYaw = fresh ? 0.f : previous.yaw - (!previous.worn.empty() && previous.worn != "off" ? 180.f : 0.f);
    const auto oldPitch = fresh ? 0.f : previous.pitch;
    if (yaw != oldYaw || next.pitch != oldPitch)
        Check(Api->Orbit(view, OrbitDelta(oldYaw, yaw), 2.f * (next.pitch - oldPitch)), "Orbit");
    if (fresh || next.zoom != previous.zoom || next.panX != previous.panX || next.panY != previous.panY) {
        const auto scale = CameraScale(next.zoom);
        // На масштабе 1 провайдер сбрасывает цель камеры.
        if (!fresh) Check(Api->Zoom(view, ZoomDelta(CameraScale(previous.zoom), 1.f), .5f, .5f), "Reset camera target");
        const auto u = scale >= .9999f ? .5f : std::clamp(.5f - next.panX * scale / (1.f - scale), 0.f, 1.f);
        const auto v = scale >= .9999f ? .5f : std::clamp(.5f - next.panY * scale / (1.f - scale), 0.f, 1.f);
        Check(Api->Zoom(view, ZoomDelta(1.f, scale), u, v), "Position camera target");
    }
}
static void OnEvent(const P::Event* event, void* cookie) noexcept {
    try {
        if (!event || event->size < offsetof(P::Event, imageUrl) + sizeof(event->imageUrl)) return;
        json update;
        {
            std::lock_guard lock(Guard);
            if (reinterpret_cast<std::uintptr_t>(cookie) != Generation || !Live.view ||
                event->actorFormId != Live.form || event->view != Live.view) return;
            if (event->kind == P::EventKind::Shown && event->imageUrl && *event->imageUrl) {
                if (Live.waiting) MoveCamera(Live.view, {}, Live, true);
                Live.waiting = false;
                Live.lastSeen = event->size >= offsetof(P::Event, lastSeen) + sizeof(event->lastSeen) && event->lastSeen;
                Live.image = event->imageUrl;
                update = Describe(Live);
            } else if (event->kind == P::EventKind::Waiting || event->kind == P::EventKind::Ended) {
                Live.waiting = event->kind == P::EventKind::Waiting;
                Live.image.clear();
                update = Describe(Live);
                if (!Live.waiting) {
                    Live.view = 0;
                    update["error"] = event->reason ? event->reason : "View ended";
                }
            } else return;
            update["generation"] = Generation;
        }
        UI::PublishPreview(std::move(update));
    } catch (...) { logger::warn("Phantasm: callback кадра отклонён"); }
}
static std::string Base64(const unsigned char *bytes, std::size_t count) {
    static constexpr char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result = "data:image/png;base64,";
    result.reserve(result.size() + ((count + 2) / 3) * 4);
    for (std::size_t i = 0; i < count; i += 3) {
        const unsigned value = (unsigned(bytes[i]) << 16) | (i + 1 < count ? unsigned(bytes[i + 1]) << 8 : 0) |
                               (i + 2 < count ? bytes[i + 2] : 0);
        result += chars[(value >> 18) & 63];
        result += chars[(value >> 12) & 63];
        result += i + 1 < count ? chars[(value >> 6) & 63] : '=';
        result += i + 2 < count ? chars[value & 63] : '=';
    }
    return result;
}

static void OnStill(const P::Still* still, void* cookie) noexcept {
    PortraitRequest request;
    try {
        {
            std::lock_guard lock(Guard);
            auto it = Portraits.find(reinterpret_cast<std::uintptr_t>(cookie));
            if (it == Portraits.end()) return;
            request = std::move(it->second);
            Portraits.erase(it);
            if (request.generation != Generation) return;
        }
        const auto publish = [&](json result) {
            result["identity"] = request.identity;
            result["kind"] = request.thumbnailToken ? "hairThumbnail" : "portrait";
            result["generation"] = request.generation;
            if (request.thumbnailToken) result["token"] = request.thumbnailToken;
            UI::PublishPreview(std::move(result));
        };
        if (!still || still->size < offsetof(P::Still, rgba) + sizeof(still->rgba)) {
            publish({{"error", "Still failed"}});
            return;
        }
        if (!still->ok || !still->rgba ||
            still->width == 0 || still->height == 0 || still->width > 1024 || still->height > 1024) {
            publish({{"error", still && still->reason ? still->reason : "Still failed"}});
            return;
        }
        // RGBA упакован без отступов и действителен только до возврата callback.
        int size = 0;
        std::vector<unsigned char> cropped;
        const auto* pixels = still->rgba;
        unsigned width = still->width, height = still->height;
        if (request.crop) {
            cropped.resize(256 * 256 * 4);
            for (unsigned y = 0; y < 256; ++y) for (unsigned x = 0; x < 256; ++x) {
                const auto sx = std::clamp(int((request.x + x / 256.f * request.size) * width), 0, int(width) - 1);
                const auto sy = std::clamp(int((request.y + y / 256.f * request.size * width / height) * height), 0, int(height) - 1);
                std::memcpy(cropped.data() + (y * 256 + x) * 4, pixels + (sy * width + sx) * 4, 4);
            }
            pixels = cropped.data(); width = height = 256;
        }
        auto* png = stbi_write_png_to_mem(pixels, width * 4, width, height, 4, &size);
        if (!png) throw std::runtime_error("Cannot encode portrait PNG");
        std::unique_ptr<unsigned char, decltype(&free)> owned(png, &free);
        auto image = Base64(png, static_cast<std::size_t>(size));
        if (request.thumbnailToken) {
            try { StoreImage(request.thumbnailCache, image); }
            catch (const std::exception& e) { logger::warn("Кэш миниатюры причёски: {}", e.what()); }
            publish({{"image", std::move(image)}});
        }
        else {
            SavePortrait(request.identity, image);
            logger::info("Портрет сохранён: {}, {} bytes", request.identity, image.size());
            publish({{"portrait", std::move(image)}});
        }
    } catch (const std::exception& failure) {
        logger::warn("Сохранение портрета: {}", failure.what());
        if (!request.identity.empty() && IsCurrent(request.generation))
            UI::PublishPreview({{"identity", request.identity}, {"kind", request.thumbnailToken ? "hairThumbnail" : "portrait"},
                {"token", request.thumbnailToken}, {"error", failure.what()}, {"generation", request.generation}});
    } catch (...) { logger::warn("Phantasm: callback портрета отклонён"); }
}
static void Initialize() {
    if (!Api) {
        const auto module = GetModuleHandleW(L"Phantasm.dll");
        if (!module) throw std::runtime_error("Enable Phantasm 1.0.0 or newer and restart Skyrim");
        const auto factory = reinterpret_cast<P::RequestPluginAPIFn>(GetProcAddress(module, "RequestPluginAPI"));
        const auto* api = factory ? static_cast<const P::Interface1*>(factory(P::kInterface1)) : nullptr;
        if (!SupportsPhantasm(api)) throw std::runtime_error("The active Phantasm DLL is too old. Use Phantasm 1.0.0 or newer");
        Api = api;
        logger::info("Превью: подключён публичный Phantasm API {}, версия {}", api->interfaceVersion, api->pluginVersion);
    }
    if (!Client) Check(Api->RegisterClient("npcstudio", OnEvent, reinterpret_cast<void*>(static_cast<std::uintptr_t>(Generation)), &Client), "RegisterClient");
}
bool IsCurrent(std::uint64_t generation) {
    std::lock_guard lock(Guard);
    return generation == Generation;
}
void Clear() {
    {
        std::lock_guard lock(Guard);
        ++Generation;
        Live = {};
        Portraits.clear();
    }
    const auto client = std::exchange(Client, 0);
    if (Api && client) Api->ReleaseClient(client);
}
static json LiveFrame(RE::FormID form, const std::string& identity, const json& options, bool item) {
    Initialize();
    {
        std::lock_guard lock(Guard);
        // Запоздавшая команда камеры не должна менять кадр во время readback.
        if (!Portraits.empty() && Portraits.begin()->second.crop) return Describe(Live);
    }
    const auto yaw = options.value("yaw", 0.f), zoom = options.value("zoom", 1.f);
    const auto pitch = std::clamp(options.value("pitch", 0.f), -75.f, 75.f);
    const auto light = options.value("light", std::string("studio"));
    const auto panX = std::clamp(options.value("panX", 0.f), -.4f, .4f);
    const auto panY = std::clamp(options.value("panY", 0.f), -.4f, .4f);
    if (!std::isfinite(panX) || !std::isfinite(panY)) throw std::runtime_error("Invalid camera offset");
    static const std::unordered_set<std::string> lights{"studio", "daylight", "hearth", "torchlight", "moonlight", "here"};
    if (!std::isfinite(yaw) || std::abs(yaw) > 36000 || !std::isfinite(pitch) || !std::isfinite(zoom) || zoom < .25f || zoom > 4.f || !lights.contains(light))
        throw std::runtime_error("Invalid preview camera");
    const auto worn = item ? options.value("worn", std::string("off")) : std::string{};
    if (item && worn != "off" && worn != "male" && worn != "female") throw std::runtime_error("Invalid armor preview mode");
    const bool motion = !item && options.value("motion", false);
    const auto kind = item ? "item" : "actor";
    LiveState previous;
    bool open;
    {
        std::lock_guard lock(Guard);
        open = !Live.view || Live.form != form || Live.identity != identity || Live.kind != kind;
        if (open) {
            if (Live.view) Api->CloseView(Live.view);
            Live = {};
        } else previous = Live;
    }
    auto next = previous;
    next.form = form;
    next.identity = identity;
    next.kind = kind;
    next.yaw = yaw;
    next.pitch = pitch;
    next.zoom = zoom;
    next.panX = panX;
    next.panY = panY;
    next.lighting = light;
    next.worn = worn;
    next.motion = motion;
    auto& view = next.view;
    if (open) {
        if (item) Check(Api->OpenItemView(Client, "item", form, 512, 512, &view), "OpenItemView");
        else Check(Api->OpenView(Client, "actor", form, 480, 640, P::Framing::Figure, &view), "OpenView");
    }
    if (!view) throw std::runtime_error("Phantasm returned an empty view");
    try {
        if (item) {
            if (worn != previous.worn) Check(Api->SetHint(view, "worn", worn.c_str()), "SetHint worn");
        } else {
            if (open) {
                Check(Api->SetHint(view, "pose", "captured"), "SetHint pose");
                Check(Api->SetHint(view, "weapons", "on"), "SetHint weapons");
            }
            if (open || motion != previous.motion) Check(Api->SetHint(view, "motion", motion ? "on" : "off"), "SetHint motion");
            if (light != previous.lighting) Check(Api->SetHint(view, "lighting", light.c_str()), "SetHint lighting");
            if (!open && options.value("capture", false)) Check(Api->Refresh(view), "Refresh");
        }
        if (!next.waiting) MoveCamera(view, previous, next, open || worn != previous.worn);
    } catch (...) {
        if (open) Api->CloseView(view);
        throw;
    }
    std::lock_guard lock(Guard);
    Live = std::move(next);
    return Describe(Live);
}
std::string ReadPortrait(const std::string& identity) {
    return ReadImage(PortraitPath(identity), 2000000);
}
json Frame(RE::Actor* actor, const std::string& identity, const json& options) {
    if (!actor || actor->IsDisabled() || actor->IsDead())
        throw std::runtime_error("NPC model is not loaded. Summon the character first");
    if (!options.value("portraitOnly", false)) return LiveFrame(actor->GetFormID(), identity, options, false);
    if (!options.value("capture", false)) {
        auto image = ReadPortrait(identity);
        if (!image.empty()) return {{"identity", identity}, {"kind", "portrait"}, {"portrait", image}};
    }
    const bool editorCapture = options.value("editorCapture", false);
    if (!editorCapture && !actor->Is3DLoaded()) throw std::runtime_error("NPC model is not loaded. Summon the character first");
    Initialize();
    std::uintptr_t ticket;
    PortraitRequest shot{identity, 0};
    if (editorCapture) {
        shot.crop = true;
        shot.x = options.value("cropX", 0.f);
        shot.y = options.value("cropY", 0.f);
        shot.size = options.value("cropSize", .5f);
        if (!std::isfinite(shot.x) || !std::isfinite(shot.y) || !std::isfinite(shot.size) ||
            shot.x < 0 || shot.y < 0 || shot.size < .05f || shot.x + shot.size > 1.0001f ||
            shot.y + shot.size * .75f > 1.0001f)
            throw std::runtime_error("Invalid portrait crop");
    }
    {
        std::lock_guard lock(Guard);
        if (!Portraits.empty()) throw std::runtime_error("Portrait queue is full");
        if (editorCapture) {
            if (!Live.view || Live.form != actor->GetFormID() || Live.identity != identity || Live.kind != "actor" || Live.image.empty())
                throw std::runtime_error("Wait for the live preview before saving the portrait");
        }
        ticket = ++NextPortrait;
        shot.generation = Generation;
        Portraits.emplace(ticket, std::move(shot));
    }
    std::uint32_t request = 0;
    // Figure снимает текущую камеру и свет. Автопортрет рисуется отдельно рядом с живым видом.
    const auto result = editorCapture ?
        Api->RequestStill(Client, actor->GetFormID(), 480, 640, P::Framing::Figure, OnStill, reinterpret_cast<void*>(ticket), &request) :
        Api->RequestStillHinted(Client, actor->GetFormID(), 144, 144, P::Framing::Portrait, "weapons=on", OnStill, reinterpret_cast<void*>(ticket), &request);
    if (result != P::Result::Ok) {
        std::lock_guard lock(Guard);
        Portraits.erase(ticket);
        Check(result, "RequestStill");
    }
    return {{"identity", identity}, {"kind", "portrait"}, {"pending", true}};
}
json ItemFrame(RE::TESBoundObject* item, const json& options) {
    if (!item || !item->Is(RE::FormType::Armor, RE::FormType::Weapon, RE::FormType::Ammo)) throw std::runtime_error("Unsupported preview item");
    if (item->Is(RE::FormType::Armor)) return LiveFrame(item->GetFormID(), fmt::format("item-{:08X}", item->GetFormID()), options, true);
    auto ground = options;
    ground["worn"] = "off";
    return LiveFrame(item->GetFormID(), fmt::format("item-{:08X}", item->GetFormID()), ground, true);
}
void CancelHairThumbnail(unsigned token) {
    std::uint32_t request = 0;
    {
        std::lock_guard lock(Guard);
        if (!token || Portraits.empty() || Portraits.begin()->second.thumbnailToken != token) return;
        request = Portraits.begin()->second.stillId;
        Portraits.erase(Portraits.begin());
    }
    if (request) Api->CancelStill(Client, request);
}
static std::filesystem::path HairThumbnailPath(std::uint64_t fingerprint) {
    return std::filesystem::path("Data/SKSE/Plugins/NPCStudio/HairThumbnails/v1") / fmt::format("{:016X}.image", fingerprint);
}
std::string ReadHairThumbnail(std::uint64_t fingerprint) {
    return ReadImage(HairThumbnailPath(fingerprint), 150000);
}
json HairThumbnail(RE::BGSHeadPart* part, unsigned token, std::uint64_t fingerprint) {
    if (!part || !token) throw std::runtime_error("Модель причёски недоступна");
    const auto identity = fmt::format("hair-{:08X}", part->GetFormID());
    const auto cache = HairThumbnailPath(fingerprint);
    if (auto image = ReadImage(cache, 150000); !image.empty())
        return {{"kind", "hairThumbnail"}, {"identity", identity}, {"token", token}, {"image", std::move(image)}};
    Initialize();
    static unsigned proxyIndex = 0;
    auto* proxy = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectMISC>(0x802 + (proxyIndex % 2), "NPCStudio.esp");
    if (!proxy) throw std::runtime_error("Обновите NPCStudio.esp для миниатюр");
    std::uintptr_t ticket;
    {
        std::lock_guard lock(Guard);
        if (!Portraits.empty()) throw std::runtime_error("busy");
        ticket = ++NextPortrait;
        Portraits.emplace(ticket, PortraitRequest{identity, Generation, 0, 0, 1, false, token});
        Portraits.at(ticket).thumbnailCache = cache;
    }
    ++proxyIndex;
    // Провайдер повторно использует модель для одинакового FormID. Чередуем две формы без ссылок в мире.
    auto path = std::string_view(part->GetModel());
    if (path.starts_with("meshes\\") || path.starts_with("Meshes\\")) path.remove_prefix(7);
    proxy->SetModel(std::string(path).c_str());
    std::uint32_t request = 0;
    const auto result = Api->RequestItemStill(Client, proxy->GetFormID(), 128, 128, "turn=180", OnStill, reinterpret_cast<void*>(ticket), &request);
    if (result != P::Result::Ok) {
        std::lock_guard lock(Guard);
        Portraits.erase(ticket);
        Check(result, "Hair thumbnail");
    }
    {
        std::lock_guard lock(Guard);
        if (auto it = Portraits.find(ticket); it != Portraits.end()) it->second.stillId = request;
    }
    return {{"kind", "hairThumbnail"}, {"pending", true}, {"identity", identity}, {"token", token}};
}
} // namespace NPCStudio::Preview
