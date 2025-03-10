#include "GameObject.h"
#include "Framework/Camera.h"
#include "Window.h"

GameObject::GameObject(Camera* InCamera)
	:
	MainCamera(InCamera)
{
}

GameObject::~GameObject()
{
}

void GameObject::BuildGameObject(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList)
{
}

void GameObject::BuildRenderItem(int& InstanceOffset, std::vector<std::unique_ptr<FrameResource>>& FrameResources)
{
	for (int i = 0; i < FrameResources.size(); i++)
	{
		UploadBuffer<AnimationData>* AnimationBuffer = FrameResources[i]->AnimationBuffer.get();

		for (int j = 0; j < Item->Animations.size(); j++)
		{
			AnimationData AnimData;
			AnimData.FinalTransform = Item->Animations[j].FinalTransform;

			AnimationBuffer->CopyData(j, AnimData);
		}
	}
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
	UpdateInstanceData(CurFrameResource);
	UpdateMaterialBuffer(CurFrameResource);
}

void GameObject::UpdateInstanceData(FrameResource* CurFrameResource)
{
	XMMATRIX View = MainCamera->GetView();
	XMMATRIX InvView = XMMatrixInverse(&XMMatrixDeterminant(View), View);

	UploadBuffer<InstanceData>* CurInstanceBuffer = CurFrameResource->InstanceBuffer.get();

	int VisibleInstanceCount = 0;

	for (int i = 0; i < Item->Instances.size(); i++)
	{
		XMMATRIX World = XMLoadFloat4x4(&Item->Instances[i].World);
		XMMATRIX TexTransform = XMLoadFloat4x4(&Item->Instances[i].TexTransform);
		XMMATRIX InvWorld = XMMatrixInverse(&XMMatrixDeterminant(World), World);
		XMMATRIX ViewToLocal = XMMatrixMultiply(InvView, InvWorld);

		BoundingFrustum LocalSpaceFrustum;
		const BoundingFrustum& CamFrustum = MainCamera->GetCameraFrustum();
		CamFrustum.Transform(LocalSpaceFrustum, ViewToLocal);

		if (LocalSpaceFrustum.Contains(Item->Bounds) != DirectX::DISJOINT)
		{
			InstanceData InstData;
			XMStoreFloat4x4(&InstData.World, XMMatrixTranspose(World));
			XMStoreFloat4x4(&InstData.TexTransform, XMMatrixTranspose(TexTransform));
			InstData.MaterialIndex = Item->Instances[i].MaterialIndex;

			CurInstanceBuffer->CopyData(Item->InstanceOffset + VisibleInstanceCount, InstData);
			++VisibleInstanceCount;
		}
	}

	Item->InstanceCount = VisibleInstanceCount;

	std::wostringstream outs;
	outs.precision(6);
	outs << L"보이는 오브젝트: " << Item->InstanceCount << L"    " << L"전체 오브젝트: " << Item->Instances.size();

	WindowManager::Get()->GetFirstWindow()->SetName(outs.str());
}

void GameObject::UpdateMaterialBuffer(FrameResource* CurFrameResource)
{
	UploadBuffer<MaterialData>* CurMaterialBuffer = CurFrameResource->MaterialBuffer.get();

	if (Mat->NumFramesDirty > 0)
	{
		XMMATRIX MatTransform = XMLoadFloat4x4(&Mat->MatTransform);

		MaterialData MatData;
		MatData.DiffuseAlbedo = Mat->DiffuseAlbedo;
		MatData.FresnelR0 = Mat->FresnelR0;
		MatData.Roughness = Mat->Roughness;
		XMStoreFloat4x4(&MatData.MatTransform, XMMatrixTranspose(MatTransform));

		CurMaterialBuffer->CopyData(Mat->MatCBIndex, MatData);

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

bool GameObject::UseAnimation() const
{
	return bUseAnimation;
}
