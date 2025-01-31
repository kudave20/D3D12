#pragma once

#include <wrl.h>
#include <vector>
#include <DirectXMath.h>
#include <d3d12.h>
#include "DX12.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct MeshGeometry;

class MathHelper;

struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& Rhs) = delete;

	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	int NumFramesDirty = gNumFrameResources;

	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

class GameObject
{
public:
	GameObject();
	virtual ~GameObject();

public:
	virtual void BuildRootSignature(ID3D12Device* Device) = 0;
	virtual void BuildGameObject(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList);
	virtual void BuildShadersAndInputLayout() = 0;
	virtual void BuildRenderItem(int ObjectIndex) = 0;
	virtual void BuildPSO(ID3D12Device* Device,
		const DXGI_FORMAT& BackBufferFormat,
		const DXGI_FORMAT& DepthStencilFormat,
		bool b4xMsaaState,
		UINT QualityOf4xMsaa);

public:
	virtual void Update(FrameResource* CurFrameResource);

private:
	void UpdateObjectCB(FrameResource* CurFrameResource);
	void UpdateMaterialCB(FrameResource* CurFrameResource);

public:
	virtual void Translate(float Dx, float Dy, float Dz);
	virtual void Rotate(float Dx, float Dy, float Dz);
	virtual void Scale(float Dx, float Dy, float Dz);

public:
	RenderItem* GetItem() const;
	ID3D12RootSignature* GetRootSignature() const;
	ID3D12PipelineState* GetPSO(const std::string& Key) const;
	int GetVertexCount() const;
	std::string GetName() const;
	bool UseMaterial() const;
	Texture* GetTexture() const;

protected:
	// FIXME: 다른데 놔두는게 좋지 않을까.
	static std::unordered_map<std::string, int> Instances;

protected:
	ComPtr<ID3DBlob> VSByteCode;
	ComPtr<ID3DBlob> PSByteCode;

	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;

	ComPtr<ID3D12RootSignature> RootSignature;

	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> PSOs;

	std::unique_ptr<MeshGeometry> Geometry;
	std::unique_ptr<RenderItem> Item;

	std::unique_ptr<Material> Mat;
	std::unique_ptr<Texture> Tex;

	int VertexCount = 0;

	RenderItem* RenderItemLayer[(int)RenderLayer::Count] = { nullptr };

	int InstanceID = 0;

protected:
	float OffsetX = 0.0f;
	float OffsetY = 0.0f;
	float OffsetZ = 0.0f;

	float Pitch = 0.0f;
	float Yaw = 0.0f;
	float Roll = 0.0f;

	float ScaleX = 1.0f;
	float ScaleY = 1.0f;
	float ScaleZ = 1.0f;
};
