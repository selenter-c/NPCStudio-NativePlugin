#include "preview/ActorPreview.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <wrl/client.h>
#include <cmath>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#pragma warning(push)
#pragma warning(disable : 4244 4267 4996)
#include "nifviewer/stb_image_write.h"
#pragma warning(pop)

namespace NPCStudio::Preview {
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using json = nlohmann::json;
struct Vertex {
    XMFLOAT3 position;
    XMFLOAT2 uv;
};
struct Mesh {
    ComPtr<ID3D11Buffer> vertices, indices;
    ComPtr<ID3D11ShaderResourceView> diffuse;
    unsigned count = 0;
    XMFLOAT4 tint{1, 1, 1, 1};
};
static ComPtr<ID3D11Device> Device;
static ComPtr<ID3D11DeviceContext> Immediate, Draw;
static ComPtr<ID3D11VertexShader> VS;
static ComPtr<ID3D11PixelShader> PS;
static ComPtr<ID3D11InputLayout> Layout;
static ComPtr<ID3D11Buffer> Constants;
static ComPtr<ID3D11SamplerState> Sampler;
static ComPtr<ID3D11RasterizerState> Raster;
static ComPtr<ID3D11DepthStencilState> Depth;
static ComPtr<ID3D11BlendState> Blend;
static ComPtr<ID3D11ShaderResourceView> White;
static std::vector<Mesh> Meshes;
static std::string Identity;
static XMFLOAT3 Minimum, Maximum;
static XMFLOAT3 Head;
static unsigned Skipped = 0;
static unsigned Visited = 0;
static unsigned CapturedVertices = 0;
static unsigned CapturedIndices = 0;
static constexpr unsigned MaxVertices = 250000;
static constexpr unsigned MaxMeshes = 256;
static constexpr auto Shader = R"(
cbuffer Constants : register(b0) { float4x4 mvp; float4 light; float4 fill; float4 tint; }
Texture2D image : register(t0); SamplerState sampleState : register(s0);
struct V { float3 p:POSITION; float2 uv:TEXCOORD; };
struct P { float4 p:SV_POSITION; float3 world:POSITION; float2 uv:TEXCOORD; };
P vs(V v) { P p; p.p=mul(float4(v.p,1),mvp); p.world=v.p; p.uv=v.uv; return p; }
float4 ps(P p):SV_TARGET {
  float4 c=image.Sample(sampleState,p.uv)*tint;
  clip(c.a-.25);
  float3 n=normalize(cross(ddy(p.world),ddx(p.world)));
  float diffuse=abs(dot(n,normalize(light.xyz)));
  float rim=abs(dot(n,normalize(float3(-light.x,light.y,.5))));
  return float4(c.rgb*(fill.rgb+diffuse*light.w+rim*fill.w),1);
})";
static void Check(HRESULT hr, const char *operation) {
    if (FAILED(hr))
        throw std::runtime_error(fmt::format("Preview {}: HRESULT {:08X}", operation, static_cast<unsigned>(hr)));
}
static void Initialize(ID3D11Device *overrideDevice = nullptr) {
    auto *device =
        overrideDevice ? overrideDevice : reinterpret_cast<ID3D11Device *>(RE::BSGraphics::Renderer::GetDevice());
    if (!device)
        throw std::runtime_error("Preview renderer is not ready");
    if (Device.Get() == device && VS)
        return;
    Device = device;
    Device->GetImmediateContext(Immediate.ReleaseAndGetAddressOf());
    Check(Device->CreateDeferredContext(0, Draw.ReleaseAndGetAddressOf()), "deferred context");
    ComPtr<ID3DBlob> vertex, pixel, errors;
    Check(D3DCompile(Shader, std::strlen(Shader), nullptr, nullptr, nullptr, "vs", "vs_5_0",
                     D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vertex, &errors),
          "vertex shader");
    Check(D3DCompile(Shader, std::strlen(Shader), nullptr, nullptr, nullptr, "ps", "ps_5_0",
                     D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pixel, &errors),
          "pixel shader");
    Check(Device->CreateVertexShader(vertex->GetBufferPointer(), vertex->GetBufferSize(), nullptr,
                                     VS.ReleaseAndGetAddressOf()),
          "vertex shader creation");
    Check(Device->CreatePixelShader(pixel->GetBufferPointer(), pixel->GetBufferSize(), nullptr,
                                    PS.ReleaseAndGetAddressOf()),
          "pixel shader creation");
    D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    Check(Device->CreateInputLayout(elements, 2, vertex->GetBufferPointer(), vertex->GetBufferSize(),
                                    Layout.ReleaseAndGetAddressOf()),
          "input layout");
    D3D11_BUFFER_DESC cb{};
    cb.ByteWidth = 112;
    cb.Usage = D3D11_USAGE_DEFAULT;
    cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Check(Device->CreateBuffer(&cb, nullptr, Constants.ReleaseAndGetAddressOf()), "constants");
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    Check(Device->CreateSamplerState(&sd, Sampler.ReleaseAndGetAddressOf()), "sampler");
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = true;
    Check(Device->CreateRasterizerState(&rd, Raster.ReleaseAndGetAddressOf()), "rasterizer");
    D3D11_DEPTH_STENCIL_DESC dd{};
    dd.DepthEnable = true;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dd.DepthFunc = D3D11_COMPARISON_LESS;
    Check(Device->CreateDepthStencilState(&dd, Depth.ReleaseAndGetAddressOf()), "depth state");
    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    Check(Device->CreateBlendState(&bd, Blend.ReleaseAndGetAddressOf()), "blend state");
    const unsigned white = 0xFFFFFFFF;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = td.Height = td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{&white, 4, 0};
    ComPtr<ID3D11Texture2D> texture;
    Check(Device->CreateTexture2D(&td, &data, &texture), "white texture");
    Check(Device->CreateShaderResourceView(texture.Get(), nullptr, White.ReleaseAndGetAddressOf()), "white view");
}
static std::vector<std::uint8_t> ReadBuffer(REX::W32::ID3D11Buffer *source, std::size_t required) {
    if (!source || !required || required > 32 * 1024 * 1024)
        return {};
    auto *buffer = reinterpret_cast<ID3D11Buffer *>(source);
    D3D11_BUFFER_DESC desc{};
    buffer->GetDesc(&desc);
    if (desc.ByteWidth < required || desc.ByteWidth > 32 * 1024 * 1024)
        return {};
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = desc.MiscFlags = desc.StructureByteStride = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Buffer> staging;
    Check(Device->CreateBuffer(&desc, nullptr, &staging), "staging buffer");
    Immediate->CopyResource(staging.Get(), buffer);
    D3D11_MAPPED_SUBRESOURCE map{};
    Check(Immediate->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &map), "read geometry");
    std::vector<std::uint8_t> bytes(required);
    std::memcpy(bytes.data(), map.pData, required);
    Immediate->Unmap(staging.Get(), 0);
    return bytes;
}
static float Half(const std::uint8_t *bytes) {
    std::uint16_t value;
    std::memcpy(&value, bytes, 2);
    return PackedVector::XMConvertHalfToFloat(value);
}
static void CaptureMesh(RE::BSTriShape *shape, const RE::NiTransform &inverse) {
    if (Meshes.size() >= MaxMeshes) {
        ++Skipped;
        return;
    }
    const auto &geometry = shape->GetGeometryRuntimeData();
    auto *skin = geometry.skinInstance.get();
    auto *partition = skin ? skin->skinPartition.get() : nullptr;
    const auto &counts = shape->GetTrishapeRuntimeData();
    auto *dynamicShape = shape->AsDynamicTriShape();
    if (partition && (partition->numPartitions > 128 || partition->partitions.size() < partition->numPartitions)) {
        ++Skipped;
        return;
    }
    const auto vertexCount = partition && partition->vertexCount ? partition->vertexCount : counts.vertexCount;
    if (!vertexCount || vertexCount > MaxVertices || CapturedVertices + vertexCount > 500000) {
        ++Skipped;
        return;
    }
    auto *buffer = geometry.rendererData;
    if (!buffer && partition && partition->numPartitions && partition->numPartitions <= 128)
        buffer = partition->partitions[0].buffData;
    if (!buffer) {
        ++Skipped;
        return;
    }
    auto desc = buffer->vertexDesc;
    std::uint64_t bits;
    std::memcpy(&bits, &desc, sizeof(bits));
    const unsigned stride = static_cast<unsigned>(bits & 15) * 4;
    if ((!dynamicShape && !desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX)) || stride < 4 || stride > 128) {
        ++Skipped;
        return;
    }
    const bool externalPosition = dynamicShape && !desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX);
    const bool fullPosition = desc.HasFlag(RE::BSGraphics::Vertex::VF_FULLPREC) ||
                              (desc.HasFlag(RE::BSGraphics::Vertex::VF_UV) &&
                               desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0) >= 16);
    const auto positionSize = externalPosition ? 0u : fullPosition ? 12u : 6u;
    if (positionSize > stride) {
        ++Skipped;
        return;
    }
    auto bytes = ReadBuffer(buffer->vertexBuffer, static_cast<std::size_t>(vertexCount) * stride);
    if (bytes.empty()) {
        ++Skipped;
        return;
    }
    std::vector<Vertex> vertices(vertexCount);
    for (unsigned i = 0; i < vertexCount; ++i) {
        auto *v = bytes.data() + i * stride;
        if (positionSize == 12)
            std::memcpy(&vertices[i].position, v, 12);
        else if (positionSize)
            vertices[i].position = {Half(v), Half(v + 2), Half(v + 4)};
        if (desc.HasFlag(RE::BSGraphics::Vertex::VF_UV)) {
            const auto offset = desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0);
            if (offset + 4 <= stride)
                vertices[i].uv = {Half(v + offset), Half(v + offset + 2)};
        }
    }
    // Данные текущей головы могут отличаться от исходного буфера после морфов.
    if (dynamicShape) {
        auto &data = dynamicShape->GetDynamicTrishapeRuntimeData();
        RE::BSSpinLockGuard guard(data.lock);
        if (data.dynamicData && data.dataSize >= vertexCount * 16) {
            for (unsigned i = 0; i < vertexCount; ++i)
                std::memcpy(&vertices[i].position, static_cast<const std::uint8_t *>(data.dynamicData) + i * 16, 12);
        } else if (externalPosition) {
            ++Skipped;
            return;
        }
    }
    const auto world = inverse * shape->world;
    if (skin) {
        auto *data = skin->skinData.get();
        if (!data || !data->boneData || !skin->bones || data->GetBoneCount() > 512 || !data->GetBoneCount()) {
            ++Skipped;
            return;
        }
        std::vector<RE::NiPoint3> transformed(vertexCount);
        std::vector<float> weights(vertexCount, 0);
        for (unsigned bone = 0; bone < data->GetBoneCount(); ++bone) {
            if (!skin->bones[bone])
                continue;
            const auto transform = inverse * skin->bones[bone]->world * data->GetBoneDataSkinToBone(bone);
            auto *values = data->GetBoneDataBoneVertData(bone);
            const auto count = data->GetBoneDataVerts(bone);
            if (!values)
                continue;
            for (unsigned n = 0; n < count; ++n) {
                const auto index = values[n].vert;
                const auto weight = values[n].weight;
                if (index >= vertexCount || !std::isfinite(weight) || weight <= 0 || weight > 1.01f)
                    continue;
                const auto &p = vertices[index].position;
                transformed[index] += (transform * RE::NiPoint3(p.x, p.y, p.z)) * weight;
                weights[index] += weight;
            }
        }
        // В SSE основные веса находятся в packed vertex stream (half4 + byte4, глобальные индексы костей).
        if (std::ranges::count_if(weights, [](float w) { return w > .5f; }) < vertexCount / 2 &&
            desc.HasFlag(RE::BSGraphics::Vertex::VF_SKINNED)) {
            const auto offset = desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING);
            if (offset + 12 <= stride) {
                std::fill(weights.begin(), weights.end(), 0.f);
                std::fill(transformed.begin(), transformed.end(), RE::NiPoint3{});
                std::vector<RE::NiTransform> transforms;
                for (unsigned bone = 0; bone < data->GetBoneCount(); ++bone)
                    transforms.push_back(skin->bones[bone]
                                             ? inverse * skin->bones[bone]->world * data->GetBoneDataSkinToBone(bone)
                                             : RE::NiTransform{});
                for (unsigned i = 0; i < vertexCount; ++i) {
                    const auto *stream = bytes.data() + i * stride + offset;
                    const auto &p = vertices[i].position;
                    for (unsigned influence = 0; influence < 4; ++influence) {
                        const auto bone = stream[8 + influence];
                        const auto weight = Half(stream + influence * 2);
                        if (bone >= transforms.size() || !skin->bones[bone] || !std::isfinite(weight) || weight <= 0 ||
                            weight > 1.01f)
                            continue;
                        transformed[i] += (transforms[bone] * RE::NiPoint3(p.x, p.y, p.z)) * weight;
                        weights[i] += weight;
                    }
                }
            }
        }
        // Не выдаём нескиненный bind pose за модель NPC, если движок освободил веса.
        if (std::ranges::count_if(weights, [](float w) { return w > .5f; }) < vertexCount / 2) {
            ++Skipped;
            return;
        }
        for (unsigned i = 0; i < vertexCount; ++i) {
            const auto &p = vertices[i].position;
            const auto value = weights[i] > .001f ? transformed[i] / weights[i] : world * RE::NiPoint3(p.x, p.y, p.z);
            vertices[i].position = {value.x, value.y, value.z};
        }
    } else {
        for (auto &v : vertices) {
            auto p = world * RE::NiPoint3(v.position.x, v.position.y, v.position.z);
            v.position = {p.x, p.y, p.z};
        }
    }
    std::vector<std::uint16_t> indices;
    auto append = [&](RE::BSGraphics::TriShape *part, unsigned triangles) {
        if (!part || !triangles || triangles > 250000)
            return;
        if (CapturedIndices + indices.size() + static_cast<std::size_t>(triangles) * 3 > 3000000)
            throw std::runtime_error("Preview geometry exceeds the index budget");
        auto raw = ReadBuffer(part->indexBuffer, static_cast<std::size_t>(triangles) * 6);
        if (raw.empty())
            return;
        auto start = indices.size();
        indices.resize(start + triangles * 3);
        std::memcpy(indices.data() + start, raw.data(), raw.size());
    };
    if (partition && partition->numPartitions <= 128) {
        RE::BSDismemberSkinInstance *dismember = nullptr;
        if (skin && std::string_view(skin->GetRTTI()->GetName()) == "BSDismemberSkinInstance")
            dismember = static_cast<RE::BSDismemberSkinInstance *>(skin);
        for (unsigned p = 0; p < partition->numPartitions; ++p) {
            if (dismember) {
                const auto &visibility = dismember->GetRuntimeData();
                if (visibility.partitions && p < static_cast<unsigned>(visibility.numPartitions) &&
                    !visibility.partitions[p].visible)
                    continue;
            }
            append(partition->partitions[p].buffData, partition->partitions[p].triangles);
        }
    } else
        append(buffer, counts.triangleCount);
    if (indices.empty() || std::ranges::any_of(indices, [vertexCount](auto index) { return index >= vertexCount; })) {
        ++Skipped;
        return;
    }
    for (const auto &v : vertices) {
        const auto &p = v.position;
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || std::abs(p.x) > 10000 ||
            std::abs(p.y) > 10000 || std::abs(p.z) > 10000) {
            ++Skipped;
            return;
        }
    }
    Mesh mesh;
    mesh.count = static_cast<unsigned>(indices.size());
    mesh.diffuse = White;
    if (auto *property = shape->lightingShaderProp_cast()) {
        auto *material = property->GetBaseMaterial();
        if (material && material->GetType() == RE::BSShaderMaterial::Type::kLighting) {
            auto *lighting = static_cast<RE::BSLightingShaderMaterialBase *>(material);
            auto *texture = lighting->diffuseTexture.get();
            if (texture && texture->rendererTexture && texture->rendererTexture->resourceView)
                mesh.diffuse = reinterpret_cast<ID3D11ShaderResourceView *>(texture->rendererTexture->resourceView);
            mesh.tint.w = lighting->materialAlpha;
            if (material->GetFeature() == RE::BSShaderMaterial::Feature::kHairTint) {
                const auto color = static_cast<RE::BSLightingShaderMaterialHairTint *>(lighting)->tintColor;
                mesh.tint.x = color.red;
                mesh.tint.y = color.green;
                mesh.tint.z = color.blue;
            } else if (material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGenRGBTint) {
                const auto color = static_cast<RE::BSLightingShaderMaterialFacegenTint *>(lighting)->tintColor;
                mesh.tint.x = color.red;
                mesh.tint.y = color.green;
                mesh.tint.z = color.blue;
            }
            for (auto &v : vertices) {
                v.uv.x = v.uv.x * material->texCoordScale[0].x + material->texCoordOffset[0].x;
                v.uv.y = v.uv.y * material->texCoordScale[0].y + material->texCoordOffset[0].y;
            }
        }
    }
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA upload{vertices.data(), 0, 0};
    Check(Device->CreateBuffer(&bd, &upload, &mesh.vertices), "mesh vertices");
    bd.ByteWidth = static_cast<UINT>(indices.size() * 2);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    upload.pSysMem = indices.data();
    Check(Device->CreateBuffer(&bd, &upload, &mesh.indices), "mesh indices");
    for (const auto index : indices) {
        auto p = vertices[index].position;
        Minimum = {std::min(Minimum.x, p.x), std::min(Minimum.y, p.y), std::min(Minimum.z, p.z)};
        Maximum = {std::max(Maximum.x, p.x), std::max(Maximum.y, p.y), std::max(Maximum.z, p.z)};
    }
    CapturedIndices += static_cast<unsigned>(indices.size());
    CapturedVertices += vertexCount;
    Meshes.push_back(std::move(mesh));
}
static void Visit(RE::NiAVObject *object, const RE::NiTransform &inverse, unsigned depth) {
    if (!object || depth > 64 || ++Visited > 4096 || object->GetAppCulled())
        return;
    if (auto *shape = object->AsTriShape())
        CaptureMesh(shape, inverse);
    if (auto *node = object->AsNode())
        for (auto &child : node->children)
            Visit(child.get(), inverse, depth + 1);
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
static std::string Render(float yaw, float zoom, unsigned light, bool portrait) {
    const unsigned width = portrait ? 144 : 600, height = portrait ? 144 : 760;
    ComPtr<ID3D11Texture2D> color, depth, staging;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = width;
    td.Height = height;
    td.ArraySize = td.MipLevels = td.SampleDesc.Count = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    Check(Device->CreateTexture2D(&td, nullptr, &color), "color target");
    ComPtr<ID3D11RenderTargetView> target;
    Check(Device->CreateRenderTargetView(color.Get(), nullptr, &target), "target view");
    td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    Check(Device->CreateTexture2D(&td, nullptr, &depth), "depth target");
    ComPtr<ID3D11DepthStencilView> z;
    Check(Device->CreateDepthStencilView(depth.Get(), nullptr, &z), "depth view");
    Draw->ClearState();
    auto *rtv = target.Get();
    Draw->OMSetRenderTargets(1, &rtv, z.Get());
    const float background[] = {.055f, .05f, .065f, 1};
    Draw->ClearRenderTargetView(target.Get(), background);
    Draw->ClearDepthStencilView(z.Get(), D3D11_CLEAR_DEPTH, 1, 0);
    D3D11_VIEWPORT viewport{0, 0, float(width), float(height), 0, 1};
    Draw->RSSetViewports(1, &viewport);
    Draw->RSSetState(Raster.Get());
    Draw->OMSetDepthStencilState(Depth.Get(), 0);
    Draw->OMSetBlendState(Blend.Get(), nullptr, 0xFFFFFFFF);
    Draw->IASetInputLayout(Layout.Get());
    Draw->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Draw->VSSetShader(VS.Get(), nullptr, 0);
    Draw->PSSetShader(PS.Get(), nullptr, 0);
    auto *cb = Constants.Get();
    Draw->VSSetConstantBuffers(0, 1, &cb);
    Draw->PSSetConstantBuffers(0, 1, &cb);
    auto *sample = Sampler.Get();
    Draw->PSSetSamplers(0, 1, &sample);
    const float extent = std::max(Maximum.z - Minimum.z, 40.f);
    const XMFLOAT3 center = portrait ? Head
                                     : XMFLOAT3{(Minimum.x + Maximum.x) * .5f, (Minimum.y + Maximum.y) * .5f,
                                                (Minimum.z + Maximum.z) * .5f};
    const float distance = (portrait ? extent * .32f : extent * 1.8f) / zoom;
    const float radians = XMConvertToRadians(portrait ? 0 : yaw);
    const auto look = XMVectorSet(center.x, center.y, center.z, 1);
    const auto eye = look + XMVectorSet(std::sin(radians) * distance, -std::cos(radians) * distance,
                                        portrait ? 0 : extent * .04f, 0);
    const auto matrix = XMMatrixLookAtLH(eye, look, XMVectorSet(0, 0, 1, 0)) *
                        XMMatrixPerspectiveFovLH(XMConvertToRadians(36), float(width) / height, 1, extent * 12);
    struct CB {
        XMFLOAT4X4 mvp;
        XMFLOAT4 light, fill, tint;
    } values;
    XMStoreFloat4x4(&values.mvp, XMMatrixTranspose(matrix));
    values.light = light == 1   ? XMFLOAT4{-1, -1, 1, .95f}
                   : light == 2 ? XMFLOAT4{1, .3f, .8f, .85f}
                                : XMFLOAT4{-.5f, -1, 1, .75f};
    values.fill = light == 1   ? XMFLOAT4{.28f, .23f, .18f, .12f}
                  : light == 2 ? XMFLOAT4{.16f, .20f, .28f, .22f}
                               : XMFLOAT4{.4f, .4f, .42f, .15f};
    for (const auto &mesh : Meshes) {
        values.tint = mesh.tint;
        Draw->UpdateSubresource(Constants.Get(), 0, nullptr, &values, 0, 0);
        UINT stride = sizeof(Vertex), offset = 0;
        auto *vb = mesh.vertices.Get();
        Draw->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        Draw->IASetIndexBuffer(mesh.indices.Get(), DXGI_FORMAT_R16_UINT, 0);
        auto *srv = mesh.diffuse.Get();
        Draw->PSSetShaderResources(0, 1, &srv);
        Draw->DrawIndexed(mesh.count, 0, 0);
    }
    ComPtr<ID3D11CommandList> commands;
    Check(Draw->FinishCommandList(false, &commands), "finish frame");
    // RestoreContextState сохраняет фактическое состояние Skyrim и его кэш настроек конвейера.
    Immediate->ExecuteCommandList(commands.Get(), true);
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.BindFlags = 0;
    td.Usage = D3D11_USAGE_STAGING;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Check(Device->CreateTexture2D(&td, nullptr, &staging), "readback texture");
    Immediate->CopyResource(staging.Get(), color.Get());
    D3D11_MAPPED_SUBRESOURCE map{};
    Check(Immediate->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &map), "readback frame");
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
    for (unsigned row = 0; row < height; ++row)
        std::memcpy(pixels.data() + row * width * 4, static_cast<const char *>(map.pData) + row * map.RowPitch,
                    width * 4);
    Immediate->Unmap(staging.Get(), 0);
    int size = 0;
    auto *png = stbi_write_png_to_mem(pixels.data(), width * 4, int(width), int(height), 4, &size);
    if (!png)
        throw std::runtime_error("Preview PNG encoding failed");
    const auto image = Base64(png, static_cast<std::size_t>(size));
    STBIW_FREE(png);
    return image;
}
void Clear() {
    Meshes.clear();
    Identity.clear();
}
json Frame(RE::Actor *actor, const std::string &identity, const json &options) {
    const float yaw = options.value("yaw", 0.f), zoom = options.value("zoom", 1.f);
    const unsigned light = options.value("light", 0u);
    if (!std::isfinite(yaw) || std::abs(yaw) > 36000 || !std::isfinite(zoom) || zoom < .5f || zoom > 2.5f || light > 2)
        throw std::runtime_error("Invalid preview camera");
    Initialize();
    const bool capture = options.value("capture", false) || Identity != identity || Meshes.empty();
    if (capture) {
        Clear();
        if (!actor || actor->IsDisabled() || !actor->Is3DLoaded())
            throw std::runtime_error("NPC model is not loaded. Summon the character first");
        auto *root = actor->Get3D();
        if (!root)
            throw std::runtime_error("NPC model is not loaded");
        Minimum = {10000, 10000, 10000};
        Maximum = {-10000, -10000, -10000};
        Skipped = Visited = CapturedVertices = CapturedIndices = 0;
        const auto inverse = root->world.Invert();
        Visit(root, inverse, 0);
        if (Meshes.empty())
            throw std::runtime_error("No supported mesh data in the loaded NPC model");
        Head = {(Minimum.x + Maximum.x) * .5f, (Minimum.y + Maximum.y) * .5f,
                Maximum.z - (Maximum.z - Minimum.z) * .08f};
        if (auto *head = root->GetObjectByName("NPC Head [Head]")) {
            auto p = inverse * head->world.translate;
            Head = {p.x, p.y, p.z + 3};
        }
        Identity = identity;
        logger::info("Превью: {} частей, {} пропущено, bounds Z {}..{}", Meshes.size(), Skipped, Minimum.z, Maximum.z);
    }
    if (options.value("portraitOnly", false))
        return {{"identity", identity}, {"portrait", Render(0, 1, 0, true)}};
    json result = {{"image", Render(yaw, zoom, light, false)},
                   {"meshes", Meshes.size()},
                   {"skipped", Skipped},
                   {"identity", identity}};
    if (capture)
        result["portrait"] = Render(0, 1, 0, true);
    return result;
}
#ifdef NPCSTUDIO_PREVIEW_TEST
void GraphicsSmoke() {
    ComPtr<ID3D11Device> device;
    D3D_FEATURE_LEVEL level;
    Check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, &level,
                            nullptr),
          "WARP device");
    Initialize(device.Get());
    Vertex vertices[] = {{{-25, 0, 0}, {0, 1}}, {{25, 0, 0}, {1, 1}}, {{0, 0, 100}, {.5f, 0}}};
    std::uint16_t indices[] = {0, 1, 2};
    Mesh mesh;
    mesh.count = 3;
    mesh.diffuse = White;
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(vertices);
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA upload{vertices, 0, 0};
    Check(Device->CreateBuffer(&bd, &upload, &mesh.vertices), "test vertices");
    bd.ByteWidth = sizeof(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    upload.pSysMem = indices;
    Check(Device->CreateBuffer(&bd, &upload, &mesh.indices), "test indices");
    Meshes.push_back(std::move(mesh));
    Minimum = {-25, 0, 0};
    Maximum = {25, 0, 100};
    Head = {0, 0, 90};
    const D3D11_VIEWPORT sentinel{7, 11, 200, 100, .2f, .8f};
    Immediate->RSSetViewports(1, &sentinel);
    const auto first = Render(0, 1, 0, false), turned = Render(40, 1, 0, false), warm = Render(0, 1, 1, false),
               portrait = Render(0, 1, 0, true);
    if (!first.starts_with("data:image/png;base64,iVBORw0KGgo") || first == turned || first == warm ||
        portrait == first)
        throw std::runtime_error("Preview did not render distinct valid PNG frames");
    D3D11_VIEWPORT current{};
    UINT count = 1;
    Immediate->RSGetViewports(&count, &current);
    if (count != 1 || std::memcmp(&current, &sentinel, sizeof(current)))
        throw std::runtime_error("Preview did not restore the graphics pipeline state");
    Clear();
}
#endif

} // namespace NPCStudio::Preview
