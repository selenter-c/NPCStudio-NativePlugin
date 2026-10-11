#pragma once

#include <d3d11.h>
#include <string>
#include <vector>
#include <cstdint>

namespace NPCStudio::NifViewer {

class NifFrameTransport {
public:
    NifFrameTransport() = default;
    ~NifFrameTransport();

    bool Initialize(ID3D11Device* device);
    void Shutdown();

    std::string CaptureFrameAsBase64(ID3D11Texture2D* colorTexture);
    
    bool IsInitialized() const { return m_initialized; }

private:
    bool CreateStagingTexture(uint32_t width, uint32_t height);
    std::string EncodePNG(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t rowPitch);
    static std::string Base64Encode(const uint8_t* data, size_t length);

    ID3D11Device* m_device = nullptr;
    ID3D11Texture2D* m_stagingTexture = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    uint32_t m_stagingWidth = 0;
    uint32_t m_stagingHeight = 0;
    bool m_initialized = false;

    std::vector<uint8_t> m_encodeBuffer;
};

}