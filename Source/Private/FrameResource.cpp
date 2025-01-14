#include "FrameResource.h"
#include "GameObject.h"

FrameResource::FrameResource(ID3D12Device* Device, UINT PassCount, UINT ObjectCount, const std::vector<GameObject*>& DynamicGameObjects)
{
    ThrowIfFailed(Device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB = std::make_unique<UploadBuffer<PassConstants>>(Device, PassCount, true);
    ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(Device, ObjectCount, true);

    for (GameObject* GameObject : DynamicGameObjects)
    {
        if (GameObject)
        {
            DynamicObjectVBMap[GameObject->GetName()].emplace_back(new UploadBuffer<Vertex>(Device, GameObject->GetVertexCount(), false));
        }
    }
}

FrameResource::~FrameResource()
{
}