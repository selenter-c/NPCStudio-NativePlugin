#include "nifviewer/NifFrameTransport.h"
#include <spdlog/spdlog.h>
#include <cstring>
#include <algorithm>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace NPCStudio::NifViewer {

static const char base64Chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

NifFrameTransport::~NifFrameTransport() {
    Shutdown();
}

bool NifFrameTransport::Initialize(ID3D11Device* device) {
    if (!device || m_initialized) return false;

    m_device = device;
    device->GetImmediateContext(&m_context);

    if (!m_context) {
        spdlog::error("Failed to get D3D11 context for frame transport");
        return false;
    }

    m_initialized = true;
    spdlog::info("NifFrameTransport initialized");
    return true;
}

void NifFrameTransport::Shutdown() {
    if (m_stagingTexture) {
        m_stagingTexture->Release();
        m_stagingTexture = nullptr;
    }
    if (m_context) {
        m_context->Release();
        m_context = nullptr;
    }
    m_device = nullptr;
    m_stagingWidth = 0;
    m_stagingHeight = 0;
    m_initialized = false;
}

bool NifFrameTransport::CreateStagingTexture(uint32_t width, uint32_t height) {
    if (m_stagingTexture && m_stagingWidth == width && m_stagingHeight == height) {
        return true;
    }

    if (m_stagingTexture) {
        m_stagingTexture->Release();
        m_stagingTexture = nullptr;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_stagingTexture);
    if (FAILED(hr)) {
        spdlog::error("Failed to create staging texture: {:08X}", hr);
        return false;
    }

    m_stagingWidth = width;
    m_stagingHeight = height;
    return true;
}

std::string NifFrameTransport::CaptureFrameAsBase64(ID3D11Texture2D* colorTexture) {
    if (!m_initialized || !colorTexture) {
        return "";
    }

    D3D11_TEXTURE2D_DESC desc;
    colorTexture->GetDesc(&desc);

    if (!CreateStagingTexture(desc.Width, desc.Height)) {
        return "";
    }

    m_context->CopyResource(m_stagingTexture, colorTexture);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = m_context->Map(m_stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        spdlog::error("Failed to map staging texture: {:08X}", hr);
        return "";
    }

    std::string result = EncodePNG(
        static_cast<const uint8_t*>(mapped.pData),
        desc.Width, desc.Height, mapped.RowPitch
    );

    m_context->Unmap(m_stagingTexture, 0);

    return result;
}

static void stbi_write_callback(void* context, void* data, int size) {
    auto* buf = static_cast<std::vector<uint8_t>*>(context);
    auto* ptr = static_cast<const uint8_t*>(data);
    buf->insert(buf->end(), ptr, ptr + size);
}

std::string NifFrameTransport::EncodePNG(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t rowPitch) {
    std::vector<uint8_t> rgba(width * height * 4);

    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t* srcRow = pixels + y * rowPitch;
        uint8_t* dstRow = rgba.data() + y * width * 4;
        
        for (uint32_t x = 0; x < width; ++x) {
            dstRow[x * 4 + 0] = srcRow[x * 4 + 0];
            dstRow[x * 4 + 1] = srcRow[x * 4 + 1];
            dstRow[x * 4 + 2] = srcRow[x * 4 + 2];
            dstRow[x * 4 + 3] = srcRow[x * 4 + 3];
        }
    }

    // Encode as real PNG
    std::vector<uint8_t> pngData;
    int result = stbi_write_png_to_func(
        stbi_write_callback,
        &pngData,
        static_cast<int>(width),
        static_cast<int>(height),
        4,
        rgba.data(),
        static_cast<int>(width * 4)
    );

    if (!result || pngData.empty()) {
        spdlog::error("PNG encoding failed");
        return "";
    }

    spdlog::info("PNG encoded: {} bytes (from {} raw bytes)", pngData.size(), rgba.size());
    return Base64Encode(pngData.data(), pngData.size());
}

std::string NifFrameTransport::Base64Encode(const uint8_t* data, size_t length) {
    std::string result;
    result.reserve(((length + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < length) {
        uint32_t octet_a = data[i++];
        uint32_t octet_b = data[i++];
        uint32_t octet_c = data[i++];
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        result += base64Chars[(triple >> 18) & 0x3F];
        result += base64Chars[(triple >> 12) & 0x3F];
        result += base64Chars[(triple >> 6) & 0x3F];
        result += base64Chars[triple & 0x3F];
    }

    if (i < length) {
        uint32_t octet_a = data[i++];
        bool hasB = (i < length);
        uint32_t octet_b = hasB ? data[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8);

        result += base64Chars[(triple >> 18) & 0x3F];
        result += base64Chars[(triple >> 12) & 0x3F];
        result += hasB ? base64Chars[(triple >> 6) & 0x3F] : '=';
        result += '=';
    }

    return result;
}

}