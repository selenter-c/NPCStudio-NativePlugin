#pragma once
#include <string>
#include <functional>

namespace NPCStudio::API {
    class PrismUIBridge {
    public:
        static PrismUIBridge& GetSingleton();
        bool Initialize();
        void* CreateView(const std::string& path);
        void ShowView(void* view, bool pauseGame = false);
        void HideView(void* view);
        void RegisterJSListener(void* view, const std::string& name, 
                                std::function<std::string(const std::string&)> cb);
        void CallJS(void* view, const std::string& fn, const std::string& json);
        bool IsAvailable() const;
    private:
        PrismUIBridge() = default;
        void* m_api = nullptr;
    };
}