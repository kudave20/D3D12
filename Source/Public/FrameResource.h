#pragma once

#include "Framework/d3dUtil.h"
#include "Framework/MathHelper.h"
#include "Framework/UploadBuffer.h"
#include <string>
#include <vector>

using namespace DirectX;

class GameObject;

struct InstanceData
{
    XMFLOAT4X4 World = MathHelper::Identity4x4();
    XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    UINT MaterialIndex;
    UINT InstancePad0;
    UINT InstancePad1;
    UINT InstancePad2;
};

struct MaterialData
{
    XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 64.0f;

    XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

    UINT DiffuseMapIndex = 0;
    UINT MaterialPad0;
    UINT MaterialPad1;
    UINT MaterialPad2;
};

struct PassConstants
{
    XMFLOAT4X4 View = MathHelper::Identity4x4();
    XMFLOAT4X4 InvView = MathHelper::Identity4x4();
    XMFLOAT4X4 Proj = MathHelper::Identity4x4();
    XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
    XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
    XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
    XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
    XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;
    XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

    Light Lights[MaxLights];
};

struct Vertex
{
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
    XMFLOAT2 TexCoord;

    bool operator==(const Vertex& Rhs) const
    {
        return (Pos.x == Rhs.Pos.x) && (Pos.y == Rhs.Pos.y) && (Pos.z == Rhs.Pos.z)
            && (Normal.x == Rhs.Normal.x) && (Normal.y == Rhs.Normal.y) && (Normal.z == Rhs.Normal.z)
            && (TexCoord.x == Rhs.TexCoord.x) && (TexCoord.y == Rhs.TexCoord.y);
    }
};

namespace std
{
    template<>
    struct hash<XMFLOAT2>
    {
    public:
        size_t operator()(const XMFLOAT2& F) const
        {
            size_t XHash = std::hash<float>()(F.x);
            size_t YHash = std::hash<float>()(F.y) << 1;
            return XHash ^ YHash;
        }
    };

    template<>
    struct hash<XMFLOAT3>
    {
    public:
        size_t operator()(const XMFLOAT3& F) const
        {
            size_t XHash = std::hash<float>()(F.x);
            size_t YHash = std::hash<float>()(F.y) << 1;
            size_t ZHash = std::hash<float>()(F.z) << 1;
            return ((XHash ^ YHash) >> 1) ^ ZHash;
        }
    };

    template <>
    struct hash<Vertex>
    {
        size_t operator()(const Vertex& V) const
        {
            size_t XHash = std::hash<XMFLOAT3>()(V.Pos);
            size_t YHash = std::hash<XMFLOAT3>()(V.Normal) << 1;
            size_t ZHash = std::hash<XMFLOAT2>()(V.TexCoord) << 1;
            return ((XHash ^ YHash) >> 1) ^ ZHash;
        }
    };
}

struct FrameResource
{
public:
    FrameResource(ID3D12Device* Device, UINT PassCount, UINT MaxInstanceCount, UINT MaterialCount);
    FrameResource(const FrameResource& Rhs) = delete;
    FrameResource& operator=(const FrameResource& Rhs) = delete;
    ~FrameResource();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;
    std::unique_ptr<UploadBuffer<InstanceData>> InstanceBuffer = nullptr;

    UINT64 Fence = 0;
};