#pragma once
#include "GameObject.h"

class Rock : public GameObject
{
public:
	Rock();
	virtual ~Rock();

public:
	virtual void BuildRootSignature(ID3D12Device* Device) override;
	virtual void BuildGameObject(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList) override;
	virtual void BuildShadersAndInputLayout() override;
	virtual void BuildRenderItem(int ObjectIndex) override;

private:
	virtual std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
};

