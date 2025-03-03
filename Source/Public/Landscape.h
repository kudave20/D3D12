#pragma once

#include "GameObject.h"

class Landscape : public GameObject
{
public:
	Landscape(Camera* InCamera);
	virtual ~Landscape();

public:
	virtual void BuildRootSignature(ID3D12Device* Device) override;
	virtual void BuildGameObject(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList) override;
	virtual void BuildShadersAndInputLayout() override;
	virtual void BuildRenderItem(int ObjectIndex) override;

private:
	float GetFloorHeight(float X, float Z);

private:
	virtual std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
};

