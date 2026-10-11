#include "nifviewer/NifRenderBackend.h"
#include "nifviewer/NifViewerConfig.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include <d3dcompiler.h>

namespace NPCStudio::NifViewer {

static const char* VS_SOURCE = R"(
cbuffer Constants : register(b0) {
    float4x4 worldViewProj;
    float4 lightDir;
    float4 lightColor;
    float4 ambientColor;
};

struct VS_INPUT {
    float3 pos : POSITION;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
};

struct VS_OUTPUT {
    float4 pos : SV_POSITION;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
    float3 lightDir : LIGHTDIR;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    output.pos = mul(float4(input.pos, 1.0f), worldViewProj);
    output.norm = input.norm;
    output.uv = input.uv;
    output.lightDir = lightDir.xyz;
    return output;
}
)";

static const char* PS_SOURCE = R"(
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
    float3 lightDir : LIGHTDIR;
};

float4 main(PS_INPUT input) : SV_TARGET {
    float3 normal = normalize(input.norm);
    float3 lightDir = normalize(input.lightDir);
    
    float diff = max(dot(normal, lightDir), 0.0f);
    
    float3 ambient = float3(0.15f, 0.15f, 0.18f);
    float3 diffuse = float3(0.7f, 0.7f, 0.75f) * diff;
    float3 color = ambient + diffuse;
    
    return float4(color, 1.0f);
}
)";

NifRenderBackend::~NifRenderBackend() {
    Shutdown();
}

bool NifRenderBackend::Initialize() {
    if (m_initialized) return true;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        &featureLevel, 1, D3D11_SDK_VERSION,
        &m_device, nullptr, &m_context
    );

    if (FAILED(hr)) {
        spdlog::error("Failed to create D3D11 device: {:08X}", hr);
        return false;
    }

    auto& config = GetConfig();
    if (!m_renderTarget.Initialize(m_device, config.defaultWidth, config.defaultHeight)) {
        spdlog::error("Failed to create render target");
        return false;
    }

    if (!CreateShaders()) {
        spdlog::error("Failed to create shaders");
        return false;
    }

    D3D11_RASTERIZER_DESC rastDesc{};
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.CullMode = D3D11_CULL_NONE;
    rastDesc.FrontCounterClockwise = FALSE;
    rastDesc.DepthClipEnable = TRUE;

    hr = m_device->CreateRasterizerState(&rastDesc, &m_rasterizerState);
    if (FAILED(hr)) {
        spdlog::error("Failed to create rasterizer state: {:08X}", hr);
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS;

    hr = m_device->CreateDepthStencilState(&depthDesc, &m_depthStencilState);
    if (FAILED(hr)) {
        spdlog::error("Failed to create depth stencil state: {:08X}", hr);
        return false;
    }

    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = m_device->CreateBlendState(&blendDesc, &m_blendState);
    if (FAILED(hr)) {
        spdlog::error("Failed to create blend state: {:08X}", hr);
        return false;
    }

    m_initialized = true;
    spdlog::info("NifRenderBackend initialized");
    return true;
}

void NifRenderBackend::Shutdown() {
    ClearModel();

    if (m_blendState) { m_blendState->Release(); m_blendState = nullptr; }
    if (m_depthStencilState) { m_depthStencilState->Release(); m_depthStencilState = nullptr; }
    if (m_rasterizerState) { m_rasterizerState->Release(); m_rasterizerState = nullptr; }
    if (m_constantBuffer) { m_constantBuffer->Release(); m_constantBuffer = nullptr; }
    if (m_inputLayout) { m_inputLayout->Release(); m_inputLayout = nullptr; }
    if (m_pixelShader) { m_pixelShader->Release(); m_pixelShader = nullptr; }
    if (m_vertexShader) { m_vertexShader->Release(); m_vertexShader = nullptr; }

    m_renderTarget.Release();

    if (m_context) { m_context->Release(); m_context = nullptr; }
    if (m_device) { m_device->Release(); m_device = nullptr; }

    m_initialized = false;
    spdlog::info("NifRenderBackend shutdown");
}

bool NifRenderBackend::CreateShaders() {
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    HRESULT hr = D3DCompile(VS_SOURCE, strlen(VS_SOURCE), nullptr, nullptr, nullptr,
                            "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    (void)errorBlob;
    if (FAILED(hr)) {
        if (errorBlob) {
            spdlog::error("VS compile error: {}", static_cast<char*>(errorBlob->GetBufferPointer()));
            errorBlob->Release();
        }
        return false;
    }

    hr = D3DCompile(PS_SOURCE, strlen(PS_SOURCE), nullptr, nullptr, nullptr,
                    "main", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    (void)errorBlob;
    if (FAILED(hr)) {
        if (errorBlob) {
            spdlog::error("PS compile error: {}", static_cast<char*>(errorBlob->GetBufferPointer()));
            errorBlob->Release();
        }
        vsBlob->Release();
        return false;
    }

    hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                      nullptr, &m_vertexShader);
    if (FAILED(hr)) {
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                     nullptr, &m_pixelShader);
    if (FAILED(hr)) {
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = m_device->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(),
                                     vsBlob->GetBufferSize(), &m_inputLayout);
    vsBlob->Release();
    psBlob->Release();

    if (FAILED(hr)) {
        spdlog::error("Failed to create input layout: {:08X}", hr);
        return false;
    }

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(ConstantBufferData);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);
    if (FAILED(hr)) {
        spdlog::error("Failed to create constant buffer: {:08X}", hr);
        return false;
    }

    return true;
}

bool NifRenderBackend::SetModel(const MeshData& mesh) {
    if (!m_initialized || mesh.vertices.empty() || mesh.indices.empty()) {
        return false;
    }

    ClearModel();

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = static_cast<UINT>(mesh.vertices.size() * sizeof(Vertex));
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = mesh.vertices.data();

    HRESULT hr = m_device->CreateBuffer(&vbDesc, &vbData, &m_vertexBuffer);
    if (FAILED(hr)) {
        spdlog::error("Failed to create vertex buffer: {:08X}", hr);
        return false;
    }

    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t));
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData{};
    ibData.pSysMem = mesh.indices.data();

    hr = m_device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer);
    if (FAILED(hr)) {
        spdlog::error("Failed to create index buffer: {:08X}", hr);
        return false;
    }

    m_indexCount = static_cast<uint32_t>(mesh.indices.size());
    m_hasModel = true;

    AutoFrameModel(mesh);

    spdlog::info("Model set: {} indices", m_indexCount);
    return true;
}

void NifRenderBackend::ClearModel() {
    if (m_indexBuffer) { m_indexBuffer->Release(); m_indexBuffer = nullptr; }
    if (m_vertexBuffer) { m_vertexBuffer->Release(); m_vertexBuffer = nullptr; }
    m_indexCount = 0;
    m_hasModel = false;
}

void NifRenderBackend::SetCamera(float yaw, float pitch, float distance) {
    m_camera.yaw = yaw;
    m_camera.pitch = std::clamp(pitch, -89.0f, 89.0f);
    m_camera.distance = std::max(0.1f, distance);
}

void NifRenderBackend::SetTarget(float x, float y, float z) {
    m_camera.targetX = x;
    m_camera.targetY = y;
    m_camera.targetZ = z;
}

void NifRenderBackend::SetFOV(float fov) {
    m_camera.fov = std::clamp(fov, 10.0f, 120.0f);
}

void NifRenderBackend::AutoFrameModel(const MeshData& mesh) {
    SetTarget(mesh.center[0], mesh.center[1], mesh.center[2]);

    float dist = mesh.radius * 2.5f;
    if (dist < 0.5f) dist = 3.0f;
    m_camera.distance = dist;
    m_camera.yaw = 45.0f;
    m_camera.pitch = -15.0f;
}

bool NifRenderBackend::Render() {
    if (!m_initialized || !m_hasModel) {
        return false;
    }

    UpdateConstantBuffer();

    ID3D11RenderTargetView* rtv = m_renderTarget.GetRTV();
    m_context->OMSetRenderTargets(1, &rtv, m_renderTarget.GetDSV());

    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(m_renderTarget.GetWidth());
    vp.Height = static_cast<float>(m_renderTarget.GetHeight());
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->ClearRenderTargetView(m_renderTarget.GetRTV(), clearColor);
    m_context->ClearDepthStencilView(m_renderTarget.GetDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetInputLayout(m_inputLayout);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    m_context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);

    m_context->VSSetShader(m_vertexShader, nullptr, 0);
    m_context->PSSetShader(m_pixelShader, nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, &m_constantBuffer);

    m_context->RSSetState(m_rasterizerState);
    m_context->OMSetDepthStencilState(m_depthStencilState, 0);
    m_context->OMSetBlendState(m_blendState, nullptr, 0xFFFFFFFF);

    m_context->DrawIndexed(m_indexCount, 0, 0);

    return true;
}

void NifRenderBackend::UpdateConstantBuffer() {
    using namespace DirectX;

    float pitchRad = XMConvertToRadians(m_camera.pitch);
    float yawRad = XMConvertToRadians(m_camera.yaw);

    float cx = cosf(pitchRad) * sinf(yawRad);
    float cy = sinf(pitchRad);
    float cz = cosf(pitchRad) * cosf(yawRad);

    XMVECTOR eyePos = XMVectorSet(
        m_camera.targetX + cx * m_camera.distance,
        m_camera.targetY + cy * m_camera.distance,
        m_camera.targetZ + cz * m_camera.distance,
        1.0f
    );

    XMVECTOR target = XMVectorSet(m_camera.targetX, m_camera.targetY, m_camera.targetZ, 1.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(eyePos, target, up);

    float aspect = static_cast<float>(m_renderTarget.GetWidth()) / 
                   static_cast<float>(m_renderTarget.GetHeight());
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_camera.fov), aspect, 0.01f, 1000.0f);

    XMMATRIX worldViewProj = XMMatrixMultiply(view, proj);

    ConstantBufferData cbData{};
    XMStoreFloat4x4(&cbData.worldViewProj, XMMatrixTranspose(worldViewProj));
    cbData.lightDir = XMFLOAT4(-0.5f, 0.8f, -0.3f, 0.0f);
    cbData.lightColor = XMFLOAT4(1.0f, 1.0f, 0.95f, 1.0f);
    cbData.ambientColor = XMFLOAT4(0.15f, 0.15f, 0.18f, 1.0f);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = m_context->Map(m_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(mapped.pData, &cbData, sizeof(cbData));
        m_context->Unmap(m_constantBuffer, 0);
    }
}

}