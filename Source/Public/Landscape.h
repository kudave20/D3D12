#pragma once

#include "GameObject.h"

class Landscape : public GameObject
{
public:
	Landscape();
	virtual ~Landscape();

public:
	virtual void BuildRootSignature(ID3D12Device* Device) override;
	virtual void BuildGameObjectGeometry(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList) override;
	virtual void BuildShadersAndInputLayout() override;
	virtual void BuildRenderItem(int ObjectIndex) override;

private:
	float GetFloorHeight(float X, float Z);
};

