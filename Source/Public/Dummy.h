#pragma once

#include "GameObject.h"

class Dummy : public GameObject
{
public:
	Dummy(Camera* InCamera);
	virtual ~Dummy();

public:
	virtual void BuildRootSignature(ID3D12Device* Device) override;
	virtual void BuildGameObject(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList) override;
	virtual void BuildShadersAndInputLayout() override;
	virtual void BuildRenderItem(int& InstanceOffset, std::vector<std::unique_ptr<FrameResource>>& FrameResources) override;

private:
	virtual std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
};

