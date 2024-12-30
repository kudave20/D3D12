#pragma once

#include "Graphics.h"
#include "Framework/d3dUtil.h"
#include "Framework/UploadBuffer.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct Vertex
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};

struct ObjectConstants
{
    XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

class DX12 : public IGraphics
{
public:
	virtual bool Init() override;
	virtual void Update() override;
	virtual void Draw() override;
	virtual void OnResize() override;

public:
	void Toggle4xMsaaState();

	float AspectRatio() const;

private:
	void CreateCommandObjects();
	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

	void FlushCommandQueue();

	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildBoxGeometry();
	void BuildPSO();

private:
	ID3D12Resource* CurrentBackBuffer() const;

	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;

public:
	void SetTheta(float InTheta);
	void SetPhi(float InPhi);
	void SetRadius(float InRadius);

public:
	float GetTheta() const;
	float GetPhi() const;
	float GetRadius() const;

private:
	ComPtr<IDXGIFactory4> DXGIFactory;
	ComPtr<IDXGISwapChain> SwapChain;
	ComPtr<ID3D12Device> D3DDevice;

	ComPtr<ID3D12Fence> Fence;
	UINT64 CurrentFence = 0;

	ComPtr<ID3D12CommandQueue> CommandQueue;
	ComPtr<ID3D12CommandAllocator> CommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> CommandList;

	ComPtr<ID3D12DescriptorHeap> RTVHeap;
	ComPtr<ID3D12DescriptorHeap> DSVHeap;
	ComPtr<ID3D12DescriptorHeap> CBVHeap;

	ComPtr<ID3D12RootSignature> RootSignature;

	ComPtr<ID3D12PipelineState> PSO;

	std::unique_ptr<MeshGeometry> BoxGeo;

	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB;

	ComPtr<ID3DBlob> VSByteCode;
    ComPtr<ID3DBlob> PSByteCode;

	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;

	bool b4xMsaaState = false;
	UINT QualityOf4xMsaa = 0;

	UINT RTVDescriptorSize = 0;
	UINT DSVDescriptorSize = 0;
	UINT CBVSRVUAVDescriptorSize = 0;

	static const int SwapChainBufferCount = 2;
	int CurrBackBuffer = 0;
	ComPtr<ID3D12Resource> SwapChainBuffer[SwapChainBufferCount];
	ComPtr<ID3D12Resource> DepthStencilBuffer;

	static const DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	D3D12_VIEWPORT ScreenViewport = {};
	D3D12_RECT ScissorRect = {};

	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 View = MathHelper::Identity4x4();
	XMFLOAT4X4 Proj = MathHelper::Identity4x4();

	float Theta = 1.5f * XM_PI;
	float Phi = XM_PIDIV4;
	float Radius = 5.0f;
};