#pragma once

#include <d3d11.h>
#include <cstdint>

namespace NPCStudio::NifViewer {

class NifRenderTarget {
public:
    NifRenderTarget() = default;
    ~NifRenderTarget();

    bool Initialize(ID3D11Device* device, uint32_t width, uint32_t height);
    void Release();
    bool Resize(ID3D11Device* device, uint32_t width, uint32_t height);

    ID3D11Texture2D* GetColorTexture() const { return m_colorTexture; }
    ID3D11Texture2D* GetDepthTexture() const { return m_depthTexture; }
    ID3D11RenderTargetView* GetRTV() const { return m_rtv; }
    ID3D11DepthStencilView* GetDSV() const { return m_dsv; }
    ID3D11ShaderResourceView* GetSRV() const { return m_srv; }

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

private:
    ID3D11Texture2D* m_colorTexture = nullptr;
    ID3D11Texture2D* m_depthTexture = nullptr;
    ID3D11RenderTargetView* m_rtv = nullptr;
    ID3D11DepthStencilView* m_dsv = nullptr;
    ID3D11ShaderResourceView* m_srv = nullptr;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

}