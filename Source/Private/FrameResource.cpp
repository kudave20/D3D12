#include "FrameResource.h"
#include "GameObject.h"

FrameResource::FrameResource(ID3D12Device* Device, UINT PassCount, UINT MaxInstanceCount, UINT MaterialCount)
{
    ThrowIfFailed(Device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB = std::make_unique<UploadBuffer<PassConstants>>(Device, PassCount, true);
    MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(Device, MaterialCount, false);
    InstanceBuffer = std::make_unique<UploadBuffer<InstanceData>>(Device, MaxInstanceCount, false);
}

FrameResource::~FrameResource()
{
}