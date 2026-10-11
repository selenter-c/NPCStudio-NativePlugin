#include "nifviewer/NifRenderTarget.h"
#include <spdlog/spdlog.h>

namespace NPCStudio::NifViewer {

NifRenderTarget::~NifRenderTarget() {
    Release();
}

bool NifRenderTarget::Initialize(ID3D11Device* device, uint32_t width, uint32_t height) {
    if (!device || width == 0 || height == 0) {
        return false;
    }

    Release();

    m_width = width;
    m_height = height;

    D3D11_TEXTURE2D_DESC colorDesc{};
    colorDesc.Width = width;
    colorDesc.Height = height;
    colorDesc.MipLevels = 1;
    colorDesc.ArraySize = 1;
    colorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    colorDesc.SampleDesc.Count = 1;
    colorDesc.Usage = D3D11_USAGE_DEFAULT;
    colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = device->CreateTexture2D(&colorDesc, nullptr, &m_colorTexture);
    if (FAILED(hr)) {
        spdlog::error("Failed to create color texture: {:08X}", hr);
        return false;
    }

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = colorDesc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    hr = device->CreateRenderTargetView(m_colorTexture, &rtvDesc, &m_rtv);
    if (FAILED(hr)) {
        spdlog::error("Failed to create RTV: {:08X}", hr);
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = colorDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(m_colorTexture, &srvDesc, &m_srv);
    if (FAILED(hr)) {
        spdlog::error("Failed to create SRV: {:08X}", hr);
        return false;
    }

    D3D11_TEXTURE2D_DESC depthDesc{};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = device->CreateTexture2D(&depthDesc, nullptr, &m_depthTexture);
    if (FAILED(hr)) {
        spdlog::error("Failed to create depth texture: {:08X}", hr);
        return false;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = depthDesc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    hr = device->CreateDepthStencilView(m_depthTexture, &dsvDesc, &m_dsv);
    if (FAILED(hr)) {
        spdlog::error("Failed to create DSV: {:08X}", hr);
        return false;
    }

    spdlog::info("Render target created: {}x{}", width, height);
    return true;
}

void NifRenderTarget::Release() {
    if (m_dsv) { m_dsv->Release(); m_dsv = nullptr; }
    if (m_depthTexture) { m_depthTexture->Release(); m_depthTexture = nullptr; }
    if (m_srv) { m_srv->Release(); m_srv = nullptr; }
    if (m_rtv) { m_rtv->Release(); m_rtv = nullptr; }
    if (m_colorTexture) { m_colorTexture->Release(); m_colorTexture = nullptr; }
    m_width = 0;
    m_height = 0;
}

bool NifRenderTarget::Resize(ID3D11Device* device, uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) {
        return true;
    }
    return Initialize(device, width, height);
}

}