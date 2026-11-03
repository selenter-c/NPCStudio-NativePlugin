#include "preview/ActorPreview.h"
#include "preview/PhantasmAPI.h"
#include "preview/PreviewMath.h"
#include "ui/MagelightBridge.h"
#include <bcrypt.h>
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
#include "nifviewer/stb_image_write.h"
#pragma warning(pop)

namespace NPCStudio::Preview {
using json = nlohmann::json;
namespace P = Phantasm;
static const P::API* Api = nullptr;
static P::Id Client = 0;
static std::mutex Guard;
static std::uint64_t Generation = 1;
struct LiveState {
    P::Id view = 0, form = 0;
    std::string identity, image, kind;
    float yaw = 0, pitch = 0, zoom = 1;
    float panX = 0, panY = 0;
    std::string lighting;
};
static LiveState Live;
struct PortraitRequest { std::string identity; std::uint64_t generation; float x = 0, y = 0, size = 1; bool crop = false; };
static std::unordered_map<std::uintptr_t, PortraitRequest> Portraits;
static std::uintptr_t NextPortrait = 0;
static std::filesystem::path PortraitPath(const std::string& identity) {
    if (identity.empty() || identity.size() > 120 || identity.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") != std::string::npos)
        throw std::runtime_error("Invalid portrait identity");
    return std::filesystem::path("Data/SKSE/Plugins/NPCStudio/Portraits") / (identity + ".image");
}
void SavePortrait(const std::string& identity, const std::string& image) {
    if (!image.starts_with("data:image/png;base64,") || image.size() > 2000000) throw std::runtime_error("Invalid portrait image");
    const auto path = PortraitPath(identity);
    std::filesystem::create_directories(path.parent_path());
    const auto temp = std::filesystem::path(path.wstring() + L".tmp");
    { std::ofstream file(temp, std::ios::binary); file << image; file.flush(); if (!file) throw std::runtime_error("Cannot save portrait"); }
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot replace portrait");
}
static json Describe(const LiveState& state) {
    return {{"identity", state.identity}, {"kind", state.kind}, {"image", state.image},
        {"pending", state.image.empty()}, {"backend", "Phantasm 0.3.0"}};
}
static void Check(P::Result result, const char* operation) {
    if (result != 0) throw std::runtime_error(fmt::format("Phantasm {}: result {}", operation, result));
}
static std::string Digest(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)), {});
    if (!stream || bytes.empty() || bytes.size() > 32 * 1024 * 1024) throw std::runtime_error("Cannot verify Phantasm.dll");
    unsigned char hash[32]{};
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE state = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("Cannot open SHA-256 provider");
    auto status = BCryptCreateHash(algorithm, &state, nullptr, 0, nullptr, 0, 0);
    if (status >= 0) status = BCryptHashData(state, bytes.data(), static_cast<ULONG>(bytes.size()), 0);
    if (status >= 0) status = BCryptFinishHash(state, hash, 32, 0);
    if (state) BCryptDestroyHash(state);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) throw std::runtime_error("Cannot hash Phantasm.dll");
    std::string result;
    for (auto value : hash) result += fmt::format("{:02x}", value);
    return result;
}
static void OnEvent(const P::Event* event, void* cookie) noexcept {
    try {
        if (!event || event->size < sizeof(P::Event)) return;
        json update;
        {
            std::lock_guard lock(Guard);
            if (reinterpret_cast<std::uintptr_t>(cookie) != Generation || !Live.form || event->form != Live.form || (Live.view && event->view != Live.view)) return;
            if (event->kind == 1 && event->imageUrl && *event->imageUrl) {
                Live.image = event->imageUrl;
                update = Describe(Live);
            } else if (event->kind == 3 && Live.view == event->view) {
                Live.image.clear();
                update = Describe(Live);
                update["error"] = event->reason ? event->reason : "View ended";
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
    try {
        PortraitRequest request;
        {
            std::lock_guard lock(Guard);
            auto it = Portraits.find(reinterpret_cast<std::uintptr_t>(cookie));
            if (it == Portraits.end()) return;
            request = std::move(it->second);
            Portraits.erase(it);
            if (request.generation != Generation) return;
        }
        if (!still || still->size < sizeof(P::Still) || !still->ok || !still->pixels ||
            still->width == 0 || still->height == 0 || still->width > 1024 || still->height > 1024) {
            logger::warn("Портрет отклонён: ok={}, size={}x{}, reason={}", still ? still->ok : false, still ? still->width : 0, still ? still->height : 0, still && still->reason ? still->reason : "no callback");
            UI::PublishPreview({{"identity", request.identity}, {"kind", "portrait"}, {"error", still && still->reason ? still->reason : "Portrait failed"}, {"generation", request.generation}});
            return;
        }
        // Readback провайдера упаковывает строки в width*4. Поле metadata не является RowPitch.
        // Пиксели принадлежат провайдеру только до возврата callback.
        int size = 0;
        std::vector<unsigned char> cropped;
        const auto* pixels = still->pixels;
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
        if (!png) return;
        std::unique_ptr<unsigned char, decltype(&free)> owned(png, &free);
        auto image = Base64(png, static_cast<std::size_t>(size));
        SavePortrait(request.identity, image);
        UI::PublishPreview({{"identity", request.identity}, {"kind", "portrait"}, {"portrait", image}, {"generation", request.generation}});
    } catch (...) { logger::warn("Phantasm: callback портрета отклонён"); }
}
static void Initialize() {
    if (!Api) {
        auto module = GetModuleHandleW(L"Phantasm.dll");
        if (!module) throw std::runtime_error("Reinstall the complete NPCStudio package including Phantasm, then restart Skyrim");
        wchar_t path[32768]{};
        if (!GetModuleFileNameW(module, path, static_cast<DWORD>(std::size(path)))) throw std::runtime_error("Cannot locate Phantasm.dll");
        if (Digest(path) != "8b9bf8341dee500c033c8777d3b616d968295676c91bbaf2946d7ae611bd7cff")
            throw std::runtime_error("Unsupported Phantasm build: expected 0.3.0 ccf916f72d59");
        auto factory = reinterpret_cast<const P::API* (*)(unsigned)>(GetProcAddress(module, "RequestPluginAPI"));
        if (!factory) throw std::runtime_error("Phantasm API export missing");
        auto* api = factory(1);
        if (!api || api->size != sizeof(P::API) || api->version != 1 || api->providerVersion != 300)
            throw std::runtime_error("Unsupported Phantasm API layout");
        const auto base = reinterpret_cast<std::uintptr_t>(module);
        // Проверяем экспортированную таблицу, а не вызываем приватные функции по смещениям.
        std::uintptr_t addresses[16]{};
        std::memcpy(addresses, reinterpret_cast<const char*>(api) + 0x10, sizeof(addresses));
        for (unsigned i = 0; i < std::size(addresses); ++i)
            if (addresses[i] != base + P::FunctionRvas[i]) throw std::runtime_error("Unexpected Phantasm API function table");
        Api = api;
        logger::info("Превью: Phantasm 0.3.0, экспортированный API 1 проверен по SHA-256 и ABI");
    }
    if (!Client) Check(Api->RegisterClient(P::ClientName, OnEvent, reinterpret_cast<void*>(static_cast<std::uintptr_t>(Generation)), &Client), "RegisterClient");
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
static json LiveFrame(P::Id form, const std::string& identity, const json& options, bool item) {
    Initialize();
    const auto yaw = options.value("yaw", 0.f), zoom = options.value("zoom", 1.f);
    const auto pitch = std::clamp(options.value("pitch", 0.f), -75.f, 75.f);
    const auto light = options.value("light", std::string("studio"));
    const auto panX = std::clamp(options.value("panX", 0.f), -.4f, .4f);
    const auto panY = std::clamp(options.value("panY", 0.f), -.4f, .4f);
    if (!std::isfinite(panX) || !std::isfinite(panY)) throw std::runtime_error("Invalid camera offset");
    static const std::unordered_set<std::string> lights{"studio", "daylight", "hearth", "torchlight", "moonlight", "here"};
    if (!std::isfinite(yaw) || std::abs(yaw) > 36000 || !std::isfinite(zoom) || zoom < .5f || zoom > 2.5f || !lights.contains(light))
        throw std::runtime_error("Invalid preview camera");
    P::Id old = 0, view = 0;
    bool open;
    float previousYaw, previousPitch, previousZoom;
    std::string previousLight;
    {
        std::lock_guard lock(Guard);
        open = !Live.view || Live.identity != identity || Live.kind != (item ? "item" : "actor");
        if (open) {
            old = Live.view;
            Live = {};
            Live.form = form; Live.identity = identity; Live.kind = item ? "item" : "actor";
        }
        view = Live.view;
        previousYaw = Live.yaw; previousPitch = Live.pitch; previousZoom = Live.zoom; previousLight = Live.lighting;
    }
    if (old) Api->CloseView(old);
    if (open) {
        if (item) Check(Api->OpenItem(Client, "item", form, 512, 512, &view), "OpenItem");
        else Check(Api->OpenActor(Client, "actor", form, 480, 640, 0, &view), "OpenActor");
        if (!item) {
            Check(Api->SetHint(view, "pose", "captured"), "SetHint pose");
            Check(Api->SetHint(view, "motion", "true"), "SetHint motion");
        }
        std::lock_guard lock(Guard);
        Live.view = view;
    }
    if (!view) throw std::runtime_error("Phantasm returned an empty view");
    // Orbit принимает градусы, Zoom — величину колеса: zoom *= pow(1.0015, delta).
    if (yaw != previousYaw || pitch != previousPitch) Check(Api->Orbit(view, OrbitDelta(previousYaw, yaw), pitch - previousPitch), "Orbit");
    const auto cameraScale = [](float value) { return .95f - (value - .5f) * .225f; };
    // При возврате scale к 1 провайдер сбрасывает цель камеры; следующий anchored zoom задаёт её заново.
    // Это перемещение внутри renderer: HTML-текстура остаётся на месте.
    float oldPanX, oldPanY;
    { std::lock_guard lock(Guard); oldPanX = Live.panX; oldPanY = Live.panY; }
    if (open || zoom != previousZoom || panX != oldPanX || panY != oldPanY) {
        const auto scale = cameraScale(zoom);
        if (!open) Check(Api->Zoom(view, ZoomDelta(cameraScale(previousZoom), 1.f), .5f, .5f), "Reset camera target");
        const auto u = std::clamp(.5f - panX * scale / (1.f - scale), 0.f, 1.f);
        const auto v = std::clamp(.5f - panY * scale / (1.f - scale), 0.f, 1.f);
        Check(Api->Zoom(view, ZoomDelta(1.f, scale), u, v), "Position camera target");
    }
    if (light != previousLight) Check(Api->SetHint(view, "lighting", light.c_str()), "SetHint");
    if (!open && options.value("capture", false)) Check(Api->Refresh(view), "Refresh");
    std::lock_guard lock(Guard);
    Live.yaw = yaw; Live.pitch = pitch; Live.zoom = zoom; Live.lighting = light;
    Live.panX = panX; Live.panY = panY;
    return Describe(Live);
}
json Frame(RE::Actor* actor, const std::string& identity, const json& options) {
    if (!actor || actor->IsDisabled() || actor->IsDead() || !actor->Is3DLoaded())
        throw std::runtime_error("NPC model is not loaded. Summon the character first");
    if (!options.value("portraitOnly", false)) return LiveFrame(actor->GetFormID(), identity, options, false);
    const auto path = PortraitPath(identity);
    if (!options.value("capture", false) && std::filesystem::exists(path) && std::filesystem::file_size(path) < 2000000) {
        std::ifstream file(path, std::ios::binary);
        std::string image((std::istreambuf_iterator<char>(file)), {});
        if (image.starts_with("data:image/png;base64,")) return {{"identity", identity}, {"kind", "portrait"}, {"portrait", image}};
    }
    Initialize();
    std::uintptr_t ticket;
    P::Id closeView = 0;
    {
        std::lock_guard lock(Guard);
        if (!Portraits.empty()) throw std::runtime_error("Portrait queue is full");
        closeView = Live.view;
        Live = {};
        ticket = ++NextPortrait;
        Portraits.emplace(ticket, PortraitRequest{identity, Generation});
        if (options.value("editorCapture", false)) {
            auto& shot = Portraits.at(ticket);
            shot.crop = true;
            shot.x = std::clamp(options.value("cropX", 0.f), 0.f, 1.f);
            shot.y = std::clamp(options.value("cropY", 0.f), 0.f, 1.f);
            shot.size = std::clamp(options.value("cropSize", .5f), .05f, 1.f);
            const auto scale = .95f - (std::clamp(options.value("zoom", 1.f), .5f, 2.5f) - .5f) * .225f;
            const auto px = std::clamp(options.value("panX", 0.f), -.4f, .4f);
            const auto py = std::clamp(options.value("panY", 0.f), -.4f, .4f);
            shot.x = .5f + (shot.x - .5f - px) * scale;
            shot.y = .5f + (shot.y - .5f - py) * scale;
            shot.size *= scale;
        }
    }
    // Still и живой вид делят renderer: очередь закрытия должна идти первой.
    if (closeView) Check(Api->CloseView(closeView), "CloseView for portrait");
    P::Id request = 0;
    const auto hints = fmt::format("turn={};pitch={};lighting={}", options.value("yaw", 0.f), options.value("pitch", 0.f), options.value("light", std::string("studio")));
    const auto result = options.value("editorCapture", false) ?
        Api->RequestStillHinted(Client, actor->GetFormID(), 480, 640, 0, hints.c_str(), OnStill, reinterpret_cast<void*>(ticket), &request) :
        Api->RequestStill(Client, actor->GetFormID(), 144, 144, 1, OnStill, reinterpret_cast<void*>(ticket), &request);
    if (result != 0) {
        std::lock_guard lock(Guard);
        Portraits.erase(ticket);
        Check(result, "RequestStill");
    }
    return {{"identity", identity}, {"kind", "portrait"}, {"pending", true}};
}
json ItemFrame(RE::TESBoundObject* item, const json& options) {
    if (!item || !item->Is(RE::FormType::Armor, RE::FormType::Weapon, RE::FormType::Ammo)) throw std::runtime_error("Unsupported preview item");
    return LiveFrame(item->GetFormID(), fmt::format("item-{:08X}", item->GetFormID()), options, true);
}
} // namespace NPCStudio::Preview
