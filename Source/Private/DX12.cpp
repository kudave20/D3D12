#include "DX12.h"
#include <WindowsX.h>
#include "Window.h"
#include "GameObject.h"
#include "FrameResource.h"
#include "Framework/GameTimer.h"
#include "Framework/Camera.h"

DX12::~DX12()
{
	if (D3DDevice)
	{
		FlushCommandQueue();
	}
}

bool DX12::Init(Camera* InCamera, std::vector<GameObject*> InStaticGameObjects, std::vector<GameObject*> InDynamicGameObjects)
{
	MainCamera = InCamera;

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

	for (GameObject* GameObject : InStaticGameObjects)
	{
		StaticGameObjects.push_back(GameObject);
	} 
	for (GameObject* GameObject : InDynamicGameObjects)
	{
		DynamicGameObjects.push_back(GameObject);
	}

	int ObjectIndex = 0;
	for (GameObject* GameObject : StaticGameObjects)
	{
		if (GameObject)
		{
			GameObject->BuildRootSignature(D3DDevice.Get());
			GameObject->BuildShadersAndInputLayout();
			GameObject->BuildGameObjectGeometry(D3DDevice.Get(), CommandList.Get());
			GameObject->BuildRenderItem(ObjectIndex);
			GameObject->BuildPSO(D3DDevice.Get(), BackBufferFormat, DepthStencilFormat, b4xMsaaState, QualityOf4xMsaa);
			++ObjectIndex;
		}
	}
	for (GameObject* GameObject : DynamicGameObjects)
	{
		if (GameObject)
		{
			GameObject->BuildRootSignature(D3DDevice.Get());
			GameObject->BuildShadersAndInputLayout();
			GameObject->BuildGameObjectGeometry(D3DDevice.Get(), CommandList.Get());
			GameObject->BuildRenderItem(ObjectIndex);
			GameObject->BuildPSO(D3DDevice.Get(), BackBufferFormat, DepthStencilFormat, b4xMsaaState, QualityOf4xMsaa);
			++ObjectIndex;
		}
	}

	BuildFrameResources();

	ThrowIfFailed(CommandList->Close());
	ID3D12CommandList* CmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(CmdsLists), CmdsLists);

	FlushCommandQueue();

	return true;
}

void DX12::Update()
{
	OnKeyboardInput();
	UpdateCamera();

	CurFrameResourceIndex = (CurFrameResourceIndex + 1) % NumFrameResources;
	CurFrameResource = FrameResources[CurFrameResourceIndex].get();

	if (CurFrameResource->Fence != 0 && Fence->GetCompletedValue() < CurFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(Fence->SetEventOnCompletion(CurFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	for (GameObject* GameObject : StaticGameObjects)
	{
		if (GameObject)
		{
			GameObject->Update(CurFrameResource);
		}
	}
	for (GameObject* GameObject : DynamicGameObjects)
	{
		if (GameObject)
		{
			GameObject->Update(CurFrameResource);
		}
	}

	UpdateMainPassConstantBuffer();
}

void DX12::Draw()
{
	ComPtr<ID3D12CommandAllocator> CmdListAlloc = CurFrameResource->CmdListAlloc;

	ThrowIfFailed(CmdListAlloc->Reset());
	ThrowIfFailed(CommandList->Reset(CmdListAlloc.Get(), nullptr));

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

	DrawItems();

	CommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(CommandList->Close());

	ID3D12CommandList* CmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(CmdsLists), CmdsLists);

	ThrowIfFailed(SwapChain->Present(0, 0));
	CurBackBuffer = (CurBackBuffer + 1) % SwapChainBufferCount;

	CurFrameResource->Fence = ++CurrentFence;

	CommandQueue->Signal(Fence.Get(), CurrentFence);
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

	CurBackBuffer = 0;

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

	MainCamera->SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
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

void DX12::BuildFrameResources()
{
	for (int i = 0; i < NumFrameResources; ++i)
	{
		FrameResources.push_back(
			std::make_unique<FrameResource>(
				D3DDevice.Get(),
				1,
				(UINT)StaticGameObjects.size() + (UINT)DynamicGameObjects.size(),
				DynamicGameObjects)
		);
	}
}

ID3D12Resource* DX12::CurrentBackBuffer() const
{
	return SwapChainBuffer[CurBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12::DepthStencilView() const
{
	return DSVHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12::CurrentBackBufferView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		RTVHeap->GetCPUDescriptorHandleForHeapStart(),
		CurBackBuffer,
		RTVDescriptorSize);
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

void DX12::DrawItems()
{
	ID3D12Resource* PassCB = CurFrameResource->PassCB->Resource();

	UINT ObjCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	ID3D12Resource* ObjectCB = CurFrameResource->ObjectCB->Resource();

	for (GameObject* GameObject : StaticGameObjects)
	{
		if (GameObject)
		{
			CommandList->SetGraphicsRootSignature(GameObject->GetRootSignature());

			RenderItem* Item = GameObject->GetItem();

			if (bIsWireFrame)
			{
				CommandList->SetPipelineState(GameObject->GetPSO("Opaque_WireFrame"));
			}
			else
			{
				CommandList->SetPipelineState(GameObject->GetPSO("Opaque"));
			}

			CommandList->IASetVertexBuffers(0, 1, &Item->Geo->VertexBufferView());
			CommandList->IASetIndexBuffer(&Item->Geo->IndexBufferView());
			CommandList->IASetPrimitiveTopology(Item->PrimitiveType);

			CommandList->SetGraphicsRootConstantBufferView(1, PassCB->GetGPUVirtualAddress());

			D3D12_GPU_VIRTUAL_ADDRESS ObjCBAddress = ObjectCB->GetGPUVirtualAddress();
			ObjCBAddress += Item->ObjCBIndex * ObjCBByteSize;

			CommandList->SetGraphicsRootConstantBufferView(0, ObjCBAddress);

			CommandList->DrawIndexedInstanced(Item->IndexCount, 1, Item->StartIndexLocation, Item->BaseVertexLocation, 0);
		}
	}
	for (GameObject* GameObject : DynamicGameObjects)
	{
		if (GameObject)
		{
			CommandList->SetGraphicsRootSignature(GameObject->GetRootSignature());

			RenderItem* Item = GameObject->GetItem();

			if (bIsWireFrame)
			{
				CommandList->SetPipelineState(GameObject->GetPSO("Opaque_WireFrame"));
			}
			else
			{
				CommandList->SetPipelineState(GameObject->GetPSO("Opaque"));
			}

			CommandList->IASetVertexBuffers(0, 1, &Item->Geo->VertexBufferView());
			CommandList->IASetIndexBuffer(&Item->Geo->IndexBufferView());
			CommandList->IASetPrimitiveTopology(Item->PrimitiveType);

			CommandList->SetGraphicsRootConstantBufferView(1, PassCB->GetGPUVirtualAddress());

			D3D12_GPU_VIRTUAL_ADDRESS ObjCBAddress = ObjectCB->GetGPUVirtualAddress();
			ObjCBAddress += Item->ObjCBIndex * ObjCBByteSize;

			CommandList->SetGraphicsRootConstantBufferView(0, ObjCBAddress);

			CommandList->DrawIndexedInstanced(Item->IndexCount, 1, Item->StartIndexLocation, Item->BaseVertexLocation, 0);
		}
	}
}

void DX12::OnKeyboardInput()
{
	if (GetAsyncKeyState('1') & 0x8000)
	{
		bIsWireFrame = true;
	}
	else
	{
		bIsWireFrame = false;
	}

	const float Dt = GameTimer::Get()->DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)
	{
		MainCamera->Walk(35.0f * Dt);
	}
	if (GetAsyncKeyState('S') & 0x8000)
	{
		MainCamera->Walk(-35.0f * Dt);
	}
	if (GetAsyncKeyState('A') & 0x8000)
	{
		MainCamera->Strafe(-35.0f * Dt);
	}
	if (GetAsyncKeyState('D') & 0x8000)
	{
		MainCamera->Strafe(35.0f * Dt);
	}
	if (GetAsyncKeyState('E') & 0x8000)
	{
		MainCamera->Fly(35.0f * Dt);
	}
	if (GetAsyncKeyState('Q') & 0x8000)
	{
		MainCamera->Fly(-35.0f * Dt);
	}

	MainCamera->UpdateViewMatrix();
}

void DX12::UpdateCamera()
{
	EyePos.x = Radius * sinf(Phi) * cosf(Theta);
	EyePos.z = Radius * sinf(Phi) * sinf(Theta);
	EyePos.y = Radius * cosf(Phi);

	XMVECTOR Pos = XMVectorSet(EyePos.x, EyePos.y, EyePos.z, 1.0f);
	XMVECTOR Target = XMVectorZero();
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX V = XMMatrixLookAtLH(Pos, Target, Up);
	XMStoreFloat4x4(&View, V);
}

void DX12::UpdateMainPassConstantBuffer()
{
	XMMATRIX NewView = MainCamera->GetView();
	XMMATRIX NewProj = MainCamera->GetProj();

	XMMATRIX NewViewProj = XMMatrixMultiply(NewView, NewProj);
	XMMATRIX NewInvView = XMMatrixInverse(&XMMatrixDeterminant(NewView), NewView);
	XMMATRIX NewInvProj = XMMatrixInverse(&XMMatrixDeterminant(NewProj), NewProj);
	XMMATRIX NewInvViewProj = XMMatrixInverse(&XMMatrixDeterminant(NewViewProj), NewViewProj);

	int Width = WindowManager::Get()->GetFirstWindow()->GetWidth();
	int Height = WindowManager::Get()->GetFirstWindow()->GetHeight();

	XMStoreFloat4x4(&MainPassCB.View, XMMatrixTranspose(NewView));
	XMStoreFloat4x4(&MainPassCB.InvView, XMMatrixTranspose(NewInvView));
	XMStoreFloat4x4(&MainPassCB.Proj, XMMatrixTranspose(NewProj));
	XMStoreFloat4x4(&MainPassCB.InvProj, XMMatrixTranspose(NewInvProj));
	XMStoreFloat4x4(&MainPassCB.ViewProj, XMMatrixTranspose(NewViewProj));
	XMStoreFloat4x4(&MainPassCB.InvViewProj, XMMatrixTranspose(NewInvViewProj));
	MainPassCB.EyePosW = MainCamera->GetPosition3f();
	MainPassCB.RenderTargetSize = XMFLOAT2((float)Width, (float)Height);
	MainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / Width, 1.0f / Height);
	MainPassCB.NearZ = 1.0f;
	MainPassCB.FarZ = 1000.0f;
	MainPassCB.TotalTime = GameTimer::Get()->TotalTime();
	MainPassCB.DeltaTime = GameTimer::Get()->DeltaTime();

	UploadBuffer<PassConstants>* CurPassCB = CurFrameResource->PassCB.get();
	CurPassCB->CopyData(0, MainPassCB);
}
