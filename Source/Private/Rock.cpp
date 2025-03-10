#include "Rock.h"
#include "FbxLoader.h"

Rock::Rock(Camera* InCamera)
	:
	GameObject(InCamera)
{
	bUseAnimation = false;
}

Rock::~Rock()
{
}

void Rock::BuildRootSignature(ID3D12Device* Device)
{
	CD3DX12_DESCRIPTOR_RANGE TexTable;
	TexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 7, 0, 0);

	CD3DX12_ROOT_PARAMETER SlotRootParameter[4];

	SlotRootParameter[0].InitAsShaderResourceView(0, 1);
	SlotRootParameter[1].InitAsShaderResourceView(1, 1);
	SlotRootParameter[2].InitAsConstantBufferView(0);
	SlotRootParameter[3].InitAsDescriptorTable(1, &TexTable, D3D12_SHADER_VISIBILITY_PIXEL);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> StaticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC RootSigDesc(4, SlotRootParameter, (UINT)StaticSamplers.size(), StaticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> SerializedRootSig = nullptr;
	ComPtr<ID3DBlob> ErrorBlob = nullptr;
	HRESULT Hr = D3D12SerializeRootSignature(&RootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		SerializedRootSig.GetAddressOf(), ErrorBlob.GetAddressOf());

	if (ErrorBlob != nullptr)
	{
		::OutputDebugStringA((char*)ErrorBlob->GetBufferPointer());
	}
	ThrowIfFailed(Hr);

	ThrowIfFailed(Device->CreateRootSignature(
		0,
		SerializedRootSig->GetBufferPointer(),
		SerializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(RootSignature.GetAddressOf())));
}

void Rock::BuildGameObject(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList)
{
	FbxLoader::Get()->Load("Models/Rock.fbx", "Rock");

	std::vector<Texture*> Textures = FbxLoader::Get()->GetTextures("Rock");

	// FIXME: 텍스쳐 여러개 받자
	Tex = std::make_unique<Texture>(*Textures[0]);

	ThrowIfFailed(CreateDDSTextureFromFile12(Device, CommandList, Tex->Filename.c_str(), Tex->Resource, Tex->UploadHeap));

	std::vector<Material*> Materials = FbxLoader::Get()->GetMaterials("Rock");

	// FIXME: 머티리얼 여러개 받을 수 있게 해야하지 않나?
	Mat = std::make_unique<Material>(*Materials[0]);

	XMFLOAT3 Minf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 Maxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR Min = XMLoadFloat3(&Minf3);
	XMVECTOR Max = XMLoadFloat3(&Maxf3);

	const std::vector<Vertex>& Vertices = FbxLoader::Get()->GetVertices("Rock");
	const UINT VBByteSize = (UINT)Vertices.size() * sizeof(Vertex);

	const std::vector<uint16_t>& Indices = FbxLoader::Get()->GetIndices("Rock");
	const UINT IBByteSize = (UINT)Indices.size() * sizeof(uint16_t);

	for (size_t i = 0; i < Vertices.size(); ++i)
	{
		const XMFLOAT3& P = Vertices[i].Pos;
		Min = XMVectorMin(Min, XMLoadFloat3(&P));
		Max = XMVectorMax(Max, XMLoadFloat3(&P));
	}

	BoundingBox Bounds;
	XMStoreFloat3(&Bounds.Center, 0.5f * (Min + Max));
	XMStoreFloat3(&Bounds.Extents, 0.5f * (Max - Min));

	std::unique_ptr<MeshGeometry> Geo = std::make_unique<MeshGeometry>();
	Geo->Name = "RockGeo";

	ThrowIfFailed(D3DCreateBlob(VBByteSize, &Geo->VertexBufferCPU));
	CopyMemory(Geo->VertexBufferCPU->GetBufferPointer(), Vertices.data(), VBByteSize);

	ThrowIfFailed(D3DCreateBlob(IBByteSize, &Geo->IndexBufferCPU));
	CopyMemory(Geo->IndexBufferCPU->GetBufferPointer(), Indices.data(), IBByteSize);

	Geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(Device, CommandList, Vertices.data(), VBByteSize, Geo->VertexBufferUploader);
	Geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(Device, CommandList, Indices.data(), IBByteSize, Geo->IndexBufferUploader);

	Geo->VertexByteStride = sizeof(Vertex);
	Geo->VertexBufferByteSize = VBByteSize;
	Geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	Geo->IndexBufferByteSize = IBByteSize;

	SubmeshGeometry Submesh;
	Submesh.IndexCount = (UINT)Indices.size();
	Submesh.StartIndexLocation = 0;
	Submesh.BaseVertexLocation = 0;
	Submesh.Bounds = Bounds;

	Geo->DrawArgs["Rock"] = Submesh;

	Geometry = std::move(Geo);

	// 순서 바꾸면 안됨
	GameObject::BuildGameObject(Device, CommandList);
}

void Rock::BuildShadersAndInputLayout()
{
	VSByteCode = d3dUtil::CompileShader(L"Source/Shader/Rock.hlsl", nullptr, "VSMain", "vs_5_1");
	PSByteCode = d3dUtil::CompileShader(L"Source/Shader/Rock.hlsl", nullptr, "PSMain", "ps_5_1");

	InputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void Rock::BuildRenderItem(int& InstanceOffset, std::vector<std::unique_ptr<FrameResource>>& FrameResources)
{
	std::unique_ptr<RenderItem> RItem = std::make_unique<RenderItem>();
	RItem->TexTransform = MathHelper::Identity4x4();
	RItem->Mat = Mat.get();
	RItem->Geo = Geometry.get();
	RItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RItem->InstanceCount = 0;
	RItem->InstanceOffset = InstanceOffset;
	RItem->IndexCount = RItem->Geo->DrawArgs["Rock"].IndexCount;
	RItem->StartIndexLocation = RItem->Geo->DrawArgs["Rock"].StartIndexLocation;
	RItem->BaseVertexLocation = RItem->Geo->DrawArgs["Rock"].BaseVertexLocation;
	RItem->Bounds = RItem->Geo->DrawArgs["Rock"].Bounds;

	const int N = 5;
	InstanceCount = N * N;
	RItem->Instances.resize(InstanceCount + RItem->InstanceOffset);

	float Width = 100.0f;
	float Depth = 100.0f;

	float X = -0.5f * Width;
	float Z = -0.5f * Depth;
	float Dx = Width / (N - 1);
	float Dz = Depth / (N - 1);

	for (int i = 0; i < N; i++)
	{
		for (int j = 0; j < N; j++)
		{
			int Index = N * i + j;

			RItem->Instances[Index].World = XMFLOAT4X4(
				ScaleX, 0.0f, 0.0f, 0.0f,
				0.0f, ScaleY, 0.0f, 0.0f,
				0.0f, 0.0f, ScaleZ, 0.0f,
				X + i * Dx, 0.0f, Z + j * Dz, 1.0f);

			XMStoreFloat4x4(&RItem->Instances[Index].TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
			RItem->Instances[Index].MaterialIndex = 0;
		}
	}

	RenderItemLayer[(int)RenderLayer::Opaque] = RItem.get();

	Item = std::move(RItem);

	InstanceOffset += InstanceCount;

	// 순서 바꾸면 안됨
	GameObject::BuildRenderItem(InstanceOffset, FrameResources);
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Rock::GetStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC PointWrap(
		0,
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);

	const CD3DX12_STATIC_SAMPLER_DESC PointClamp(
		1,
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	const CD3DX12_STATIC_SAMPLER_DESC LinearWrap(
		2,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);

	const CD3DX12_STATIC_SAMPLER_DESC LinearClamp(
		3,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	const CD3DX12_STATIC_SAMPLER_DESC AnisotropicWrap(
		4,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		0.0f,
		8);

	const CD3DX12_STATIC_SAMPLER_DESC AnisotropicClamp(
		5,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		0.0f,
		8);

	return {
		PointWrap, PointClamp,
		LinearWrap, LinearClamp,
		AnisotropicWrap, AnisotropicClamp };
}
