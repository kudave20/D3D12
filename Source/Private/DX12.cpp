#include "DX12.h"
#include <WindowsX.h>
#include "Window.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

bool DX12::Init()
{
#if defined(DEBUG) || defined(_DEBUG)
	ComPtr<ID3D12Debug> DebugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController)));
	DebugController->EnableDebugLayer();
#endif

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&DXGIFactory)));

	HRESULT HardwareResult = D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&D3DDevice));

	if (FAILED(HardwareResult))
	{
		ComPtr<IDXGIAdapter> WarpAdapter;
		ThrowIfFailed(DXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(&WarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			WarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&D3DDevice)));
	}

	ThrowIfFailed(D3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)));

	RTVDescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	DSVDescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	CBVSRVUAVDescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS MSQualityLevels;
	ZeroMemory(&MSQualityLevels, sizeof(MSQualityLevels));
	MSQualityLevels.Format = BackBufferFormat;
	MSQualityLevels.SampleCount = 4;
	MSQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	MSQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(D3DDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&MSQualityLevels,
		sizeof(MSQualityLevels)));

	QualityOf4xMsaa = MSQualityLevels.NumQualityLevels;
	assert(QualityOf4xMsaa > 0);

#ifdef _DEBUG
	LogAdapters();
#endif

	CreateCommandObjects();
	CreateSwapChain();
	CreateRtvAndDsvDescriptorHeaps();

	OnResize();

	ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));

	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildBoxGeometry();
	BuildPSO();

	ThrowIfFailed(CommandList->Close());
	ID3D12CommandList* CmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(CmdsLists), CmdsLists);

	FlushCommandQueue();

	return true;
}

void DX12::Update()
{
	float X = Radius * sinf(Phi) * cosf(Theta);
	float Z = Radius * sinf(Phi) * sinf(Theta);
	float Y = Radius * cosf(Phi);

	XMVECTOR Pos = XMVectorSet(X, Y, Z, 1.0f);
	XMVECTOR Target = XMVectorZero();
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX V = XMMatrixLookAtLH(Pos, Target, Up);
	XMStoreFloat4x4(&View, V);

	XMMATRIX W = XMLoadFloat4x4(&World);
	XMMATRIX P = XMLoadFloat4x4(&Proj);
	XMMATRIX WVP = W * V * P;

	ObjectConstants ObjConstants;
	XMStoreFloat4x4(&ObjConstants.WorldViewProj, XMMatrixTranspose(WVP));
	ObjectCB->CopyData(0, ObjConstants);
}

void DX12::Draw()
{
	ThrowIfFailed(CommandAllocator->Reset());

	ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), PSO.Get()));

	CommandList->RSSetViewports(1, &ScreenViewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	CommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET));

	CommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	CommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	CommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { CBVHeap.Get() };
	CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	CommandList->SetGraphicsRootSignature(RootSignature.Get());

	CommandList->IASetVertexBuffers(0, 1, &BoxGeo->VertexBufferView());
	CommandList->IASetIndexBuffer(&BoxGeo->IndexBufferView());
	CommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	CommandList->SetGraphicsRootDescriptorTable(0, CBVHeap->GetGPUDescriptorHandleForHeapStart());

	CommandList->DrawIndexedInstanced(
		BoxGeo->DrawArgs["box"].IndexCount,
		1, 0, 0, 0);

	CommandList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(CommandList->Close());

	ID3D12CommandList* CmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(CmdsLists), CmdsLists);

	ThrowIfFailed(SwapChain->Present(0, 0));
	CurrBackBuffer = (CurrBackBuffer + 1) % SwapChainBufferCount;

	FlushCommandQueue();
}

void DX12::OnResize()
{
	assert(D3DDevice);
	assert(SwapChain);
	assert(CommandAllocator);

	FlushCommandQueue();

	ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));

	for (int i = 0; i < SwapChainBufferCount; i++)
	{
		SwapChainBuffer[i].Reset();
	}
	DepthStencilBuffer.Reset();

	int Width = WindowManager::Get()->GetFirstWindow()->GetWidth();
	int Height = WindowManager::Get()->GetFirstWindow()->GetHeight();

	ThrowIfFailed(SwapChain->ResizeBuffers(
		SwapChainBufferCount,
		Width, Height,
		BackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	CurrBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHeapHandle(RTVHeap->GetCPUDescriptorHandleForHeapStart());

	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		ThrowIfFailed(SwapChain->GetBuffer(i, IID_PPV_ARGS(&SwapChainBuffer[i])));
		D3DDevice->CreateRenderTargetView(SwapChainBuffer[i].Get(), nullptr, RTVHeapHandle);
		RTVHeapHandle.Offset(1, RTVDescriptorSize);
	}

	D3D12_RESOURCE_DESC DepthStencilDesc;
	ZeroMemory(&DepthStencilDesc, sizeof(DepthStencilDesc));
	DepthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	DepthStencilDesc.Alignment = 0;
	DepthStencilDesc.Width = Width;
	DepthStencilDesc.Height = Height;
	DepthStencilDesc.DepthOrArraySize = 1;
	DepthStencilDesc.MipLevels = 1;
	DepthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	DepthStencilDesc.SampleDesc.Count = b4xMsaaState ? 4 : 1;
	DepthStencilDesc.SampleDesc.Quality = b4xMsaaState ? (QualityOf4xMsaa - 1) : 0;
	DepthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	DepthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE OptClear;
	ZeroMemory(&OptClear, sizeof(OptClear));
	OptClear.Format = DepthStencilFormat;
	OptClear.DepthStencil.Depth = 1.0f;
	OptClear.DepthStencil.Stencil = 0;
	ThrowIfFailed(D3DDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&DepthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&OptClear,
		IID_PPV_ARGS(DepthStencilBuffer.GetAddressOf())));

	D3D12_DEPTH_STENCIL_VIEW_DESC DSVDesc;
	ZeroMemory(&DSVDesc, sizeof(DSVDesc));
	DSVDesc.Flags = D3D12_DSV_FLAG_NONE;
	DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DSVDesc.Format = DepthStencilFormat;
	DSVDesc.Texture2D.MipSlice = 0;
	D3DDevice->CreateDepthStencilView(DepthStencilBuffer.Get(), &DSVDesc, DepthStencilView());

	CommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			DepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_DEPTH_WRITE));

	ThrowIfFailed(CommandList->Close());
	ID3D12CommandList* CmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(CmdsLists), CmdsLists);

	FlushCommandQueue();

	ScreenViewport.TopLeftX = 0;
	ScreenViewport.TopLeftY = 0;
	ScreenViewport.Width = static_cast<float>(Width);
	ScreenViewport.Height = static_cast<float>(Height);
	ScreenViewport.MinDepth = 0.0f;
	ScreenViewport.MaxDepth = 1.0f;

	ScissorRect = { 0, 0, Width, Height };

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&Proj, P);
}

void DX12::Toggle4xMsaaState()
{
	b4xMsaaState = !b4xMsaaState;

	CreateSwapChain();
	OnResize();
}

float DX12::AspectRatio() const
{
	int Width = WindowManager::Get()->GetFirstWindow()->GetWidth();
	int Height = WindowManager::Get()->GetFirstWindow()->GetHeight();
	return static_cast<float>(Width) / Height;
}

void DX12::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(D3DDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&CommandQueue)));

	ThrowIfFailed(D3DDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CommandAllocator.GetAddressOf())));

	ThrowIfFailed(D3DDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		CommandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(CommandList.GetAddressOf())));

	CommandList->Close();
}

void DX12::CreateSwapChain()
{
	SwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC Sd;
	ZeroMemory(&Sd, sizeof(Sd));
	Sd.BufferDesc.Width = WindowManager::Get()->GetFirstWindow()->GetWidth();
	Sd.BufferDesc.Height = WindowManager::Get()->GetFirstWindow()->GetHeight();
	Sd.BufferDesc.RefreshRate.Numerator = 60;
	Sd.BufferDesc.RefreshRate.Denominator = 1;
	Sd.BufferDesc.Format = BackBufferFormat;
	Sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	Sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	Sd.SampleDesc.Count = b4xMsaaState ? 4 : 1;
	Sd.SampleDesc.Quality = b4xMsaaState ? (QualityOf4xMsaa - 1) : 0;
	Sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Sd.BufferCount = SwapChainBufferCount;
	Sd.OutputWindow = WindowManager::Get()->GetFirstWindow()->GetWindow();
	Sd.Windowed = true;
	Sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	Sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(DXGIFactory->CreateSwapChain(
		CommandQueue.Get(),
		&Sd,
		SwapChain.GetAddressOf()));
}

void DX12::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc;
	ZeroMemory(&RTVHeapDesc, sizeof(RTVHeapDesc));
	RTVHeapDesc.NumDescriptors = SwapChainBufferCount;
	RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	RTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	RTVHeapDesc.NodeMask = 0;
	ThrowIfFailed(D3DDevice->CreateDescriptorHeap(&RTVHeapDesc, IID_PPV_ARGS(RTVHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC DSVHeapDesc;
	ZeroMemory(&DSVHeapDesc, sizeof(DSVHeapDesc));
	DSVHeapDesc.NumDescriptors = 1;
	DSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	DSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DSVHeapDesc.NodeMask = 0;
	ThrowIfFailed(D3DDevice->CreateDescriptorHeap(&DSVHeapDesc, IID_PPV_ARGS(DSVHeap.GetAddressOf())));
}

void DX12::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (DXGIFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i)
	{
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

void DX12::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, BackBufferFormat);

		ReleaseCom(output);

		++i;
	}
}

void DX12::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;

	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (const auto& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
			L"\n";

		::OutputDebugString(text.c_str());
	}
}

void DX12::FlushCommandQueue()
{
	CurrentFence++;

	ThrowIfFailed(CommandQueue->Signal(Fence.Get(), CurrentFence));

	if (Fence->GetCompletedValue() < CurrentFence)
	{
		HANDLE EventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		ThrowIfFailed(Fence->SetEventOnCompletion(CurrentFence, EventHandle));

		WaitForSingleObject(EventHandle, INFINITE);
		CloseHandle(EventHandle);
	}
}

void DX12::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC CBVHeapDesc;
	CBVHeapDesc.NumDescriptors = 1;
	CBVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	CBVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	CBVHeapDesc.NodeMask = 0;
	ThrowIfFailed(D3DDevice->CreateDescriptorHeap(&CBVHeapDesc, IID_PPV_ARGS(&CBVHeap)));
}

void DX12::BuildConstantBuffers()
{
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(D3DDevice.Get(), 1, true);

	UINT ObjCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	D3D12_GPU_VIRTUAL_ADDRESS CbAddress = ObjectCB->Resource()->GetGPUVirtualAddress();
	int BoxCBufIndex = 0;
	CbAddress += BoxCBufIndex * ObjCBByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc;
	CBVDesc.BufferLocation = CbAddress;
	CBVDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	D3DDevice->CreateConstantBufferView(&CBVDesc, CBVHeap->GetCPUDescriptorHandleForHeapStart());
}

void DX12::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER SlotRootParameter[1];

	CD3DX12_DESCRIPTOR_RANGE CBVTable;
	CBVTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	SlotRootParameter[0].InitAsDescriptorTable(1, &CBVTable);

	CD3DX12_ROOT_SIGNATURE_DESC RootSigDesc(1, SlotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> SerializedRootSig = nullptr;
	ComPtr<ID3DBlob> ErrorBlob = nullptr;
	HRESULT Hr = D3D12SerializeRootSignature(&RootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		SerializedRootSig.GetAddressOf(), ErrorBlob.GetAddressOf());

	if (ErrorBlob != nullptr)
	{
		::OutputDebugStringA((char*)ErrorBlob->GetBufferPointer());
	}
	ThrowIfFailed(Hr);

	ThrowIfFailed(D3DDevice->CreateRootSignature(
		0,
		SerializedRootSig->GetBufferPointer(),
		SerializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&RootSignature)));
}

void DX12::BuildShadersAndInputLayout()
{
	HRESULT Hr = S_OK;

	VSByteCode = d3dUtil::CompileShader(L"Source/Shader/Box.hlsl", nullptr, "VSMain", "vs_5_0");
	PSByteCode = d3dUtil::CompileShader(L"Source/Shader/Box.hlsl", nullptr, "PSMain", "ps_5_0");

	InputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void DX12::BuildBoxGeometry()
{
	std::array<Vertex, 8> Vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};

	std::array<std::uint16_t, 36> indices =
	{
		0, 1, 2,
		0, 2, 3,

		4, 6, 5,
		4, 7, 6,

		4, 5, 1,
		4, 1, 0,

		3, 2, 6,
		3, 6, 7,

		1, 5, 6,
		1, 6, 2,

		4, 0, 3,
		4, 3, 7
	};

	const UINT VBByteSize = (UINT)Vertices.size() * sizeof(Vertex);
	const UINT IBByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	BoxGeo = std::make_unique<MeshGeometry>();
	BoxGeo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(VBByteSize, &BoxGeo->VertexBufferCPU));
	CopyMemory(BoxGeo->VertexBufferCPU->GetBufferPointer(), Vertices.data(), VBByteSize);

	ThrowIfFailed(D3DCreateBlob(IBByteSize, &BoxGeo->IndexBufferCPU));
	CopyMemory(BoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), IBByteSize);

	BoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
		D3DDevice.Get(),
		CommandList.Get(), 
		Vertices.data(), 
		VBByteSize, 
		BoxGeo->VertexBufferUploader);

	BoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
		D3DDevice.Get(),
		CommandList.Get(), 
		indices.data(), 
		IBByteSize, 
		BoxGeo->IndexBufferUploader);

	BoxGeo->VertexByteStride = sizeof(Vertex);
	BoxGeo->VertexBufferByteSize = VBByteSize;
	BoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	BoxGeo->IndexBufferByteSize = IBByteSize;

	SubmeshGeometry Submesh;
	Submesh.IndexCount = (UINT)indices.size();
	Submesh.StartIndexLocation = 0;
	Submesh.BaseVertexLocation = 0;

	BoxGeo->DrawArgs["box"] = Submesh;
}

void DX12::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc;
	ZeroMemory(&PSODesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	PSODesc.InputLayout = { InputLayout.data(), (UINT)InputLayout.size() };
	PSODesc.pRootSignature = RootSignature.Get();
	PSODesc.VS =
	{
		reinterpret_cast<BYTE*>(VSByteCode->GetBufferPointer()),
		VSByteCode->GetBufferSize()
	};
	PSODesc.PS =
	{
		reinterpret_cast<BYTE*>(PSByteCode->GetBufferPointer()),
		PSByteCode->GetBufferSize()
	};
	PSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	PSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	PSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	PSODesc.SampleMask = UINT_MAX;
	PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	PSODesc.NumRenderTargets = 1;
	PSODesc.RTVFormats[0] = BackBufferFormat;
	PSODesc.SampleDesc.Count = b4xMsaaState ? 4 : 1;
	PSODesc.SampleDesc.Quality = b4xMsaaState ? (QualityOf4xMsaa - 1) : 0;
	PSODesc.DSVFormat = DepthStencilFormat;
	ThrowIfFailed(D3DDevice->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&PSO)));
}

ID3D12Resource* DX12::CurrentBackBuffer() const
{
	return SwapChainBuffer[CurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12::DepthStencilView() const
{
	return DSVHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12::CurrentBackBufferView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		RTVHeap->GetCPUDescriptorHandleForHeapStart(),
		CurrBackBuffer,
		RTVDescriptorSize);
}

void DX12::SetTheta(float InTheta)
{
	Theta = InTheta;
}

void DX12::SetPhi(float InPhi)
{
	Phi = InPhi;
}

void DX12::SetRadius(float InRadius)
{
	Radius = InRadius;
}

float DX12::GetTheta() const
{
	return Theta;
}

float DX12::GetPhi() const
{
	return Phi;
}

float DX12::GetRadius() const
{
	return Radius;
}
