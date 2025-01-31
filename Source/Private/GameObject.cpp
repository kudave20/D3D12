#include "GameObject.h"

std::unordered_map<std::string, int> GameObject::Instances;

GameObject::GameObject()
{
}

GameObject::~GameObject()
{
}

void GameObject::BuildGameObject(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList)
{
	int& InstanceCount = Instances[GetName()];
	InstanceID = InstanceCount;
	++InstanceCount;
}

void GameObject::BuildPSO(ID3D12Device* Device, const DXGI_FORMAT& BackBufferFormat, const DXGI_FORMAT& DepthStencilFormat, bool b4xMsaaState, UINT QualityOf4xMsaa)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC OpaquePSODesc;
	ZeroMemory(&OpaquePSODesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	OpaquePSODesc.InputLayout = { InputLayout.data(), (UINT)InputLayout.size() };
	OpaquePSODesc.pRootSignature = RootSignature.Get();
	OpaquePSODesc.VS =
	{
		reinterpret_cast<BYTE*>(VSByteCode->GetBufferPointer()), VSByteCode->GetBufferSize()
	};
	OpaquePSODesc.PS =
	{
		reinterpret_cast<BYTE*>(PSByteCode->GetBufferPointer()), PSByteCode->GetBufferSize()
	};
	OpaquePSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	OpaquePSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	OpaquePSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	OpaquePSODesc.SampleMask = UINT_MAX;
	OpaquePSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	OpaquePSODesc.NumRenderTargets = 1;
	OpaquePSODesc.RTVFormats[0] = BackBufferFormat;
	OpaquePSODesc.SampleDesc.Count = b4xMsaaState ? 4 : 1;
	OpaquePSODesc.SampleDesc.Quality = b4xMsaaState ? (QualityOf4xMsaa - 1) : 0;
	OpaquePSODesc.DSVFormat = DepthStencilFormat;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&OpaquePSODesc, IID_PPV_ARGS(&PSOs["Opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC OpaqueWireFramePSODesc = OpaquePSODesc;
	OpaqueWireFramePSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&OpaqueWireFramePSODesc, IID_PPV_ARGS(&PSOs["Opaque_WireFrame"])));
}

void GameObject::Update(FrameResource* CurFrameResource)
{
	UpdateObjectCB(CurFrameResource);
	UpdateMaterialCB(CurFrameResource);
}

void GameObject::UpdateObjectCB(FrameResource* CurFrameResource)
{
	UploadBuffer<ObjectConstants>* CurObjectCB = CurFrameResource->ObjectCB.get();

	if (Item->NumFramesDirty > 0)
	{
		XMMATRIX World = XMLoadFloat4x4(&Item->World);
		XMMATRIX TWorld = XMMatrixMultiply(World, XMMatrixTranslation(OffsetX, OffsetY, OffsetZ));
		XMMATRIX TRWorld = XMMatrixMultiply(TWorld, XMMatrixRotationRollPitchYaw(Pitch, Yaw, Roll));
		XMMATRIX TRSWorld = XMMatrixMultiply(TRWorld, XMMatrixScaling(ScaleX, ScaleY, ScaleZ));

		XMMATRIX TexTransform = XMLoadFloat4x4(&Item->TexTransform);

		ObjectConstants ObjConstants;
		XMStoreFloat4x4(&ObjConstants.World, XMMatrixTranspose(TRSWorld));
		XMStoreFloat4x4(&ObjConstants.TexTransform, XMMatrixTranspose(TexTransform));

		CurObjectCB->CopyData(Item->ObjCBIndex, ObjConstants);

		--Item->NumFramesDirty;
	}
}

void GameObject::UpdateMaterialCB(FrameResource* CurFrameResource)
{
	if (nullptr == Mat)
	{
		return;
	}

	UploadBuffer<MaterialConstants>* CurMaterialCB = CurFrameResource->MaterialCB.get();

	if (Mat->NumFramesDirty > 0)
	{
		XMMATRIX MatTransform = XMLoadFloat4x4(&Mat->MatTransform);

		MaterialConstants MatConstants;
		MatConstants.DiffuseAlbedo = Mat->DiffuseAlbedo;
		MatConstants.FresnelR0 = Mat->FresnelR0;
		MatConstants.Roughness = Mat->Roughness;
		XMStoreFloat4x4(&MatConstants.MatTransform, XMMatrixTranspose(MatTransform));

		CurMaterialCB->CopyData(Mat->MatCBIndex, MatConstants);

		--Mat->NumFramesDirty;
	}
}

void GameObject::Translate(float Dx, float Dy, float Dz)
{
	OffsetX = Dx;
	OffsetY = Dy;
	OffsetZ = Dz;
}

void GameObject::Rotate(float Dx, float Dy, float Dz)
{
	Pitch = Dx;
	Yaw = Dy;
	Roll = Dz;
}

void GameObject::Scale(float Dx, float Dy, float Dz)
{
	ScaleX = Dx;
	ScaleY = Dy;
	ScaleZ = Dz;
}

RenderItem* GameObject::GetItem() const
{
	return Item.get();
}

ID3D12RootSignature* GameObject::GetRootSignature() const
{
	return RootSignature.Get();
}

ID3D12PipelineState* GameObject::GetPSO(const std::string& Key) const
{
    auto It = PSOs.find(Key);
    return PSOs.end() != It ? It->second.Get() : nullptr;
}

int GameObject::GetVertexCount() const
{
    return VertexCount;
}

std::string GameObject::GetName() const
{
    return Geometry->Name;
}

Texture* GameObject::GetTexture() const
{
	return Tex.get();
}
