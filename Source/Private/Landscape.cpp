#include "Landscape.h"
#include "FrameResource.h"
#include "Framework/GeometryGenerator.h"
#include "Framework/d3dUtil.h"

Landscape::Landscape(Camera* InCamera)
	:
	GameObject(InCamera)
{
}

Landscape::~Landscape()
{
}

void Landscape::BuildRootSignature(ID3D12Device* Device)
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

void Landscape::BuildGameObject(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList)
{
	GeometryGenerator GeometryGenerator;
	GeometryGenerator::MeshData Grid = GeometryGenerator.CreateGrid(160.0f, 160.0f, 41, 41);

	Mat = std::make_unique<Material>();
	Mat->Name = "LandMat";
	Mat->MatCBIndex = 0;
	Mat->DiffuseSrvHeapIndex = 0;
	Mat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	Mat->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	Mat->Roughness = 0.3f;

	Tex = std::make_unique<Texture>();
	Tex->Filename = L"Textures/texture_ground.dds";
	Tex->Name = "LandTex";
	ThrowIfFailed(CreateDDSTextureFromFile12(Device, CommandList, Tex->Filename.c_str(), Tex->Resource, Tex->UploadHeap));

	XMFLOAT3 Minf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 Maxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR Min = XMLoadFloat3(&Minf3);
	XMVECTOR Max = XMLoadFloat3(&Maxf3);

	std::vector<Vertex> Vertices(Grid.Vertices.size());
	for (size_t i = 0; i < Grid.Vertices.size(); ++i)
	{
		XMFLOAT3& P = Grid.Vertices[i].Position;
		Vertices[i].Pos = P;
		Vertices[i].Pos.y = GetFloorHeight(P.x, P.z);
		Vertices[i].Normal = Grid.Vertices[i].Normal;
		Vertices[i].TexCoord.x = Grid.Vertices[i].TexC.x * 10.0f;
		Vertices[i].TexCoord.y = Grid.Vertices[i].TexC.y * 10.0f;

		Min = XMVectorMin(Min, XMLoadFloat3(&P));
		Max = XMVectorMax(Max, XMLoadFloat3(&P));
	}

	BoundingBox Bounds;
	XMStoreFloat3(&Bounds.Center, 0.5f * (Min + Max));
	XMStoreFloat3(&Bounds.Extents, 0.5f * (Max - Min));

	VertexCount = (int)Grid.Vertices.size();

	const UINT VBByteSize = (UINT)Vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> Indices = Grid.GetIndices16();
	const UINT IBByteSize = (UINT)Indices.size() * sizeof(std::uint16_t);

	std::unique_ptr<MeshGeometry> Geo = std::make_unique<MeshGeometry>();
	Geo->Name = "LandGeo";

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

	Geo->DrawArgs["Grid"] = Submesh;

	Geometry = std::move(Geo);

	// 순서 바꾸면 안됨
	GameObject::BuildGameObject(Device, CommandList);
}

void Landscape::BuildShadersAndInputLayout()
{
	VSByteCode = d3dUtil::CompileShader(L"Source/Shader/Landscape.hlsl", nullptr, "VSMain", "vs_5_1");
	PSByteCode = d3dUtil::CompileShader(L"Source/Shader/Landscape.hlsl", nullptr, "PSMain", "ps_5_1");

	InputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void Landscape::BuildRenderItem(int ObjectIndex)
{
	std::unique_ptr<RenderItem> RItem = std::make_unique<RenderItem>();
	RItem->TexTransform = MathHelper::Identity4x4();
	RItem->Mat = Mat.get();
	RItem->Geo = Geometry.get();
	RItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RItem->InstanceCount = 0;
	RItem->InstanceOffset = ObjectIndex;
	RItem->IndexCount = RItem->Geo->DrawArgs["Grid"].IndexCount;
	RItem->StartIndexLocation = RItem->Geo->DrawArgs["Grid"].StartIndexLocation;
	RItem->BaseVertexLocation = RItem->Geo->DrawArgs["Grid"].BaseVertexLocation;
	RItem->Bounds = RItem->Geo->DrawArgs["Grid"].Bounds;

	RItem->Instances.resize(1 + RItem->InstanceOffset);
	XMStoreFloat4x4(&RItem->Instances[0].TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	RItem->Instances[0].MaterialIndex = 0;

	RenderItemLayer[(int)RenderLayer::Opaque] = RItem.get();

	Item = std::move(RItem);
}

float Landscape::GetFloorHeight(float X, float Z)
{
	return 0.0f;

	//return 0.03f * (Z * sinf(0.1f * X) + X * cosf(0.1f * Z));
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Landscape::GetStaticSamplers()
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
