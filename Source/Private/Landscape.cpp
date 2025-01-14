#include "Landscape.h"
#include "FrameResource.h"
#include "Framework/GeometryGenerator.h"
#include "Framework/d3dUtil.h"

Landscape::Landscape()
{
}

Landscape::~Landscape()
{
}

void Landscape::BuildRootSignature(ID3D12Device* Device)
{
	CD3DX12_ROOT_PARAMETER SlotRootParameter[2];

	SlotRootParameter[0].InitAsConstantBufferView(0);
	SlotRootParameter[1].InitAsConstantBufferView(1);

	CD3DX12_ROOT_SIGNATURE_DESC RootSigDesc(2, SlotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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

void Landscape::BuildGameObjectGeometry(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList)
{
	GeometryGenerator GeometryGenerator;
	GeometryGenerator::MeshData Grid = GeometryGenerator.CreateGrid(160.0f, 160.0f, 50, 50);

	std::vector<Vertex> Vertices(Grid.Vertices.size());
	for (size_t i = 0; i < Grid.Vertices.size(); ++i)
	{
		XMFLOAT3& P = Grid.Vertices[i].Position;
		Vertices[i].Pos = P;
		Vertices[i].Pos.y = GetFloorHeight(P.x, P.z);
	}

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

	Geo->DrawArgs["Grid"] = Submesh;

	Geometry = std::move(Geo);

	// 순서 바꾸면 안됨
	GameObject::BuildGameObjectGeometry(Device, CommandList);
}

void Landscape::BuildShadersAndInputLayout()
{
	VSByteCode = d3dUtil::CompileShader(L"Source/Shader/Landscape.hlsl", nullptr, "VSMain", "vs_5_0");
	PSByteCode = d3dUtil::CompileShader(L"Source/Shader/Landscape.hlsl", nullptr, "PSMain", "ps_5_0");

	InputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void Landscape::BuildRenderItem(int ObjectIndex)
{
	std::unique_ptr<RenderItem> RItem = std::make_unique<RenderItem>();
	RItem->World = MathHelper::Identity4x4();
	RItem->ObjCBIndex = ObjectIndex;
	RItem->Geo = Geometry.get();
	RItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RItem->IndexCount = RItem->Geo->DrawArgs["Grid"].IndexCount;
	RItem->StartIndexLocation = RItem->Geo->DrawArgs["Grid"].StartIndexLocation;
	RItem->BaseVertexLocation = RItem->Geo->DrawArgs["Grid"].BaseVertexLocation;

	RenderItemLayer[(int)RenderLayer::Opaque] = RItem.get();

	Item = std::move(RItem);
}

float Landscape::GetFloorHeight(float X, float Z)
{
	return 0.03f * (Z * sinf(0.1f * X) + X * cosf(0.1f * Z));
}
