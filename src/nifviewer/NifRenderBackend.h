#pragma once

#include "NifRenderTarget.h"
#include "NifModelLoader.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <cstdint>
#include <vector>

namespace NPCStudio::NifViewer {

struct CameraState {
    float yaw = 0.0f;
    float pitch = -15.0f;
    float distance = 3.0f;
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 0.0f;
    float fov = 45.0f;
};

class NifRenderBackend {
public:
    NifRenderBackend() = default;
    ~NifRenderBackend();

    bool Initialize();
    void Shutdown();

    bool SetModel(const MeshData& mesh);
    void ClearModel();

    void SetCamera(float yaw, float pitch, float distance);
    void SetTarget(float x, float y, float z);
    void SetFOV(float fov);
    void AutoFrameModel(const MeshData& mesh);

    bool Render();

    ID3D11Device* GetDevice() const { return m_device; }
    ID3D11DeviceContext* GetContext() const { return m_context; }
    NifRenderTarget& GetRenderTarget() { return m_renderTarget; }

    bool IsInitialized() const { return m_initialized; }
    bool HasModel() const { return m_hasModel; }

private:
    bool CreateShaders();
    bool CreateBuffers();
    void UpdateConstantBuffer();

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    NifRenderTarget m_renderTarget;

    ID3D11VertexShader* m_vertexShader = nullptr;
    ID3D11PixelShader* m_pixelShader = nullptr;
    ID3D11InputLayout* m_inputLayout = nullptr;
    ID3D11Buffer* m_vertexBuffer = nullptr;
    ID3D11Buffer* m_indexBuffer = nullptr;
    ID3D11Buffer* m_constantBuffer = nullptr;
    ID3D11RasterizerState* m_rasterizerState = nullptr;
    ID3D11DepthStencilState* m_depthStencilState = nullptr;
    ID3D11BlendState* m_blendState = nullptr;

    uint32_t m_indexCount = 0;
    bool m_initialized = false;
    bool m_hasModel = false;

    CameraState m_camera;

    struct ConstantBufferData {
        DirectX::XMFLOAT4X4 worldViewProj;
        DirectX::XMFLOAT4 lightDir;
        DirectX::XMFLOAT4 lightColor;
        DirectX::XMFLOAT4 ambientColor;
    };
};

}