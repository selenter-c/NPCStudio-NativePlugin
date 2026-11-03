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
    float yaw = 0, zoom = 1;
    std::string lighting;
};
static LiveState Live;
struct PortraitRequest { std::string identity; std::uint64_t generation; };
static std::unordered_map<std::uintptr_t, PortraitRequest> Portraits;
static std::uintptr_t NextPortrait = 0;
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
            !ValidPortrait(still->width, still->height)) return;
        // Readback провайдера упаковывает строки в width*4. Поле metadata не является RowPitch.
        // Пиксели принадлежат провайдеру только до возврата callback.
        int size = 0;
        auto* png = stbi_write_png_to_mem(still->pixels, 144 * 4, 144, 144, 4, &size);
        if (!png) return;
        std::unique_ptr<unsigned char, decltype(&free)> owned(png, &free);
        auto image = Base64(png, static_cast<std::size_t>(size));
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
    const auto light = options.value("light", std::string("studio"));
    static const std::unordered_set<std::string> lights{"studio", "daylight", "hearth", "torchlight", "moonlight", "here"};
    if (!std::isfinite(yaw) || std::abs(yaw) > 36000 || !std::isfinite(zoom) || zoom < .5f || zoom > 2.5f || !lights.contains(light))
        throw std::runtime_error("Invalid preview camera");
    P::Id old = 0, view = 0;
    bool open;
    float previousYaw, previousZoom;
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
        previousYaw = Live.yaw; previousZoom = Live.zoom; previousLight = Live.lighting;
    }
    if (old) Api->CloseView(old);
    if (open) {
        if (item) Check(Api->OpenItem(Client, "item", form, 512, 512, &view), "OpenItem");
        else Check(Api->OpenActor(Client, "actor", form, 480, 640, 0, &view), "OpenActor");
        std::lock_guard lock(Guard);
        Live.view = view;
    }
    if (!view) throw std::runtime_error("Phantasm returned an empty view");
    // Orbit принимает градусы, Zoom — величину колеса: zoom *= pow(1.0015, delta).
    if (yaw != previousYaw) Check(Api->Orbit(view, OrbitDelta(previousYaw, yaw), 0), "Orbit");
    if (zoom != previousZoom) Check(Api->Zoom(view, ZoomDelta(previousZoom, zoom), .5f, .5f), "Zoom");
    if (light != previousLight) Check(Api->SetHint(view, "lighting", light.c_str()), "SetHint");
    if (!open && options.value("capture", false)) Check(Api->Refresh(view), "Refresh");
    std::lock_guard lock(Guard);
    Live.yaw = yaw; Live.zoom = zoom; Live.lighting = light;
    return Describe(Live);
}
json Frame(RE::Actor* actor, const std::string& identity, const json& options) {
    if (!actor || actor->IsDisabled() || actor->IsDead() || !actor->Is3DLoaded())
        throw std::runtime_error("NPC model is not loaded. Summon the character first");
    if (!options.value("portraitOnly", false)) return LiveFrame(actor->GetFormID(), identity, options, false);
    Initialize();
    std::uintptr_t ticket;
    {
        std::lock_guard lock(Guard);
        if (Portraits.size() >= 4) throw std::runtime_error("Portrait queue is full");
        ticket = ++NextPortrait;
        Portraits.emplace(ticket, PortraitRequest{identity, Generation});
    }
    P::Id request = 0;
    const auto result = Api->RequestStill(Client, actor->GetFormID(), 144, 144, 1, OnStill, reinterpret_cast<void*>(ticket), &request);
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
