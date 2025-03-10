#include "FbxLoader.h"
#include <cassert>
#include <queue>
#include "Framework/MathHelper.h"

FbxLoader* FbxLoader::Loader = nullptr;

FbxLoader::FbxLoader()
{
	assert(Loader == nullptr);
	Loader = this;
}

FbxLoader::~FbxLoader()
{
}

FbxLoader* FbxLoader::Get()
{
	return Loader;
}

void FbxLoader::Init()
{
	Manager = FbxManager::Create();

	IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
	Manager->SetIOSettings(IOSettings);
}

bool FbxLoader::Load(const char* FilePath, const std::string& Name)
{
	Importer = FbxImporter::Create(Manager, "");

	if (false == Importer->Initialize(FilePath, -1, Manager->GetIOSettings()))
	{
		return false;
	}

	Textures[Name].clear();
	Materials[Name].clear();
	Vertices[Name].clear();
	Indices[Name].clear();

	FbxScene* Scene = FbxScene::Create(Manager, "My Scene");
	Importer->Import(Scene);
	Importer->Destroy();

	FbxAxisSystem::MayaYUp.ConvertScene(Scene);

	FbxGeometryConverter Converter(Manager);
	Converter.Triangulate(Scene, true);

	LoadTexture(FilePath, Scene, Name);
	LoadMaterial(Scene, Name);
	FbxMesh* Mesh = LoadMesh(Scene, Name);
	LoadAnimation(Scene, Name, Mesh);

	return true;
}

void FbxLoader::LoadTexture(const char* FilePath, FbxScene* Scene, const std::string& Name)
{
	int TextureCount = Scene->GetTextureCount();
	for (int i = 0; i < TextureCount; ++i)
	{
		FbxTexture* Tex = Scene->GetTexture(i);
		std::unique_ptr<Texture> Data = std::make_unique<Texture>();
		Data->Name = Tex->GetName();
		Data->Filename = ConvertToTextureName(FilePath, Data->Name);
		Data->Index = (int)Textures.size();
		Textures[Name].push_back(std::move(Data));
	}
}

void FbxLoader::LoadMaterial(FbxScene* Scene, const std::string& Name)
{
	int MaterialCount = Scene->GetMaterialCount();

	for (int i = 0; i < MaterialCount; ++i)
	{
		FbxSurfaceMaterial* SurfaceMaterial = Scene->GetMaterial(i);
		FbxClassId ClassID = SurfaceMaterial->GetClassId();
		FbxProperty Property = SurfaceMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);

		std::unique_ptr<Material> Mat = std::make_unique<Material>();
		FbxTexture* Tex = Property.GetSrcObject<FbxTexture>(0);

		if (Tex)
		{
			// FIXME: unordered_map 쓰는게 나을지도..
			Mat->DiffuseSrvHeapIndex = Textures[Name][i]->Index;
		}
		else
		{
			Mat->DiffuseSrvHeapIndex = 0;
		}

		if (ClassID.Is(FbxSurfacePhong::ClassId))
		{
			FbxSurfacePhong* Phong = (FbxSurfacePhong*)(SurfaceMaterial);
			XMFLOAT3 DiffuseColor = MathHelper::Fbx4ToXM3(Phong->Diffuse.Get());

			Mat->DiffuseAlbedo = XMFLOAT4(DiffuseColor.x, DiffuseColor.y, DiffuseColor.z, 1.0f);
			Mat->Name = Phong->GetName();
		}
		else if (ClassID.Is(FbxSurfaceLambert::ClassId))
		{
			FbxSurfaceLambert* Lambert = (FbxSurfaceLambert*)(SurfaceMaterial);
			XMFLOAT3 DiffuseColor = MathHelper::Fbx4ToXM3(Lambert->Diffuse.Get());

			Mat->DiffuseAlbedo = XMFLOAT4(DiffuseColor.x, DiffuseColor.y, DiffuseColor.z, 1.0f);
			Mat->Name = Lambert->GetName();
		}

		Mat->MatCBIndex = (int)Materials.size();

		// FIXME: 하드코딩 없애자
		Mat->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
		Mat->Roughness = 0.3f;

		Materials[Name].push_back(std::move(Mat));
	}
}

FbxMesh* FbxLoader::LoadMesh(FbxScene* Scene, const std::string& Name)
{
	FbxNode* RootNode = Scene->GetRootNode();
	FbxMesh* Mesh = nullptr;
	if (false == FindMesh(RootNode, Mesh))
	{
		return nullptr;
	}

	ProcessPolygon(Mesh, Name);

	return Mesh;
}

void FbxLoader::LoadAnimation(FbxScene* Scene, const std::string& Name, FbxMesh* Mesh)
{
	FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(0);
	if (nullptr == AnimStack)
	{
		return;
	}

	FbxString AnimStackName = AnimStack->GetName();

	FbxSkin* Skin = reinterpret_cast<FbxSkin*>(Mesh->GetDeformer(0, FbxDeformer::eSkin));
	int BoneCount = Skin->GetClusterCount();
	BoneCounts[Name] = BoneCount;

	FbxAMatrix GeometryTransform = GetGeometryTransformation(Mesh->GetNode());

	for (int i = 0; i < BoneCount; i++)
	{
		FbxCluster* Cluster = Skin->GetCluster(i);
		int* VertexList = Cluster->GetControlPointIndices();
		int VertexCount = Cluster->GetControlPointIndicesCount();
		for (int j = 0; j < VertexCount; j++)
		{
			const FbxVector4& Pos = Mesh->GetControlPointAt(VertexList[j]);

			// FIXME: 어차피 Vertex끼리 비교할 때 Pos만 비교하니까 이렇게 했다.
			Vertex Vertex;
			Vertex.Pos = MathHelper::Fbx4ToXM3(Pos);

			uint16_t Index = IndexMap[Vertex];
			for (int k = 0; k < 4; k++)
			{
				// FIXME: 255번째 Bone이 있으면 망한다.
				if (Vertices[Name][Index].BoneIndices[k] == 255)
				{
					Vertices[Name][Index].BoneIndices[k] = i;
					float BoneWeight = (float)Cluster->GetControlPointWeights()[j];

					switch (k)
					{
					case 0:
						Vertices[Name][Index].BoneWeights.x = BoneWeight;
						break;
					case 1:
						Vertices[Name][Index].BoneWeights.y = BoneWeight;
						break;
					case 2:
						Vertices[Name][Index].BoneWeights.z = BoneWeight;
						break;
					case 3:
						Vertices[Name][Index].BoneWeights.w = BoneWeight;
						break;
					}

					break;
				}
			}
		}

		FbxAMatrix TransformMatrix;
		Cluster->GetTransformMatrix(TransformMatrix);
		FbxAMatrix TransformLinkMatrix;
		Cluster->GetTransformLinkMatrix(TransformLinkMatrix);
		FbxAMatrix BoneOffsetMatrix = TransformLinkMatrix.Inverse() * TransformMatrix * GeometryTransform;

		XMFLOAT4X4 BoneOffset;
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 4; ++j)
			{
				BoneOffset.m[i][j] = BoneOffsetMatrix.Get(i, j);
			}
		}
		BoneOffsets[Name].push_back(XMLoadFloat4x4(&BoneOffset));
	}

	FbxTakeInfo* TakeInfo = Scene->GetTakeInfo(AnimStackName);
	FbxTime Start = TakeInfo->mLocalTimeSpan.GetStart();
	FbxTime End = TakeInfo->mLocalTimeSpan.GetStop();
	
	FbxLongLong DebugStart = Start.GetFrameCount(FbxTime::eFrames24);
	FbxLongLong DebugEnd = End.GetFrameCount(FbxTime::eFrames24);

	for (FbxLongLong i = Start.GetFrameCount(FbxTime::eFrames24); i <= End.GetFrameCount(FbxTime::eFrames24); i++)
	{
		FindAnimation(Scene, Mesh, i, Name);
	}
}

std::wstring FbxLoader::ConvertToTextureName(const char* FilePath, const std::string& Name)
{
	std::string FileName(FilePath);
	FileName = FileName.substr(7, FileName.size() - (7 + 4));

	std::string TextureName(Name);
	if (TextureName.find("_jpg") != std::string::npos)
	{
		size_t Index = TextureName.find("_jpg");
		TextureName = TextureName.replace(Index, Index + 4, ".dds");
	}

	if (TextureName.find(".dds") == std::string::npos)
	{
		TextureName.append(".dds");
	}

	std::stringstream Stream;
	Stream << "Textures/" << TextureName;

	std::string Str = Stream.str();
	std::wstring Wstr;
	Wstr.assign(Str.begin(), Str.end());

	return Wstr;
}

bool FbxLoader::FindMesh(FbxNode* Node, FbxMesh*& OutMesh)
{
	FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
	if (Attribute)
	{
		if (Attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			OutMesh = Node->GetMesh();
			return true;
		}
	}

	int ChildCount = Node->GetChildCount();
	for (int i = 0; i < ChildCount; i++)
	{
		if (FindMesh(Node->GetChild(i), OutMesh))
		{
			return true;
		}
	}

	return false;
}

void FbxLoader::FindAnimation(FbxScene* Scene, FbxMesh* Mesh, const FbxLongLong& Time, const std::string& Name)
{
	FbxAnimEvaluator* AnimEvaluator = Scene->GetAnimationEvaluator();

	FbxSkin* Skin = reinterpret_cast<FbxSkin*>(Mesh->GetDeformer(0, FbxDeformer::eSkin));
	int ClusterCount = Skin->GetClusterCount();
	for (int i = 0; i < ClusterCount; i++)
	{
		FbxCluster* Cluster = Skin->GetCluster(i);
		FbxNode* Node = Cluster->GetLink();

		FbxTime CurTime;
		CurTime.SetFrame(Time, FbxTime::eFrames24);

		FbxAMatrix RootGlobalMatrix = Mesh->GetNode()->EvaluateGlobalTransform(CurTime);
		FbxAMatrix ToRootMatrix = RootGlobalMatrix.Inverse() * Node->EvaluateGlobalTransform(CurTime);
		
		XMFLOAT4 Translation = MathHelper::Fbx4ToXM4(ToRootMatrix.GetT());
		XMFLOAT4 Quaternion = MathHelper::QuatToXM4(ToRootMatrix.GetQ());
		XMFLOAT4 Scale = MathHelper::Fbx4ToXM4(ToRootMatrix.GetS());

		XMVECTOR TranslationVector = XMLoadFloat4(&Translation);
		XMVECTOR QuaternionVector = XMLoadFloat4(&Quaternion);
		XMVECTOR ScaleVector = XMLoadFloat4(&Scale);
		XMVECTOR Zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

		XMMATRIX ToRootTransform = XMMatrixAffineTransformation(ScaleVector, Zero, QuaternionVector, TranslationVector);
		ToRootTransforms[Name].push_back(ToRootTransform);
	}
}

void FbxLoader::ProcessPolygon(FbxMesh* Mesh, const std::string& Name)
{
	int PolygonCount = Mesh->GetPolygonCount();

	for (int i = 0; i < PolygonCount; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			Vertex Vertex;
			int Index = Mesh->GetPolygonVertex(i, j);

			GetPosition(Mesh, Index, Vertex.Pos);
			GetNormal(Mesh, i, j, Vertex.Normal);
			GetTexCoord(Mesh, i, j, Vertex.TexCoord);

			Vertex.BoneWeights.x = 0.0f;
			Vertex.BoneWeights.y = 0.0f;
			Vertex.BoneWeights.z = 0.0f;
			Vertex.BoneWeights.w = 0.0f;

			Vertex.BoneIndices[0] = 255;
			Vertex.BoneIndices[1] = 255;
			Vertex.BoneIndices[2] = 255;
			Vertex.BoneIndices[3] = 255;

			InsertVertex(Vertex, Name);
		}
	}
}

void FbxLoader::InsertVertex(const Vertex& InVertex, const std::string& Name)
{
	auto Lookup = IndexMap.find(InVertex);
	if (Lookup != IndexMap.end())
	{
		Indices[Name].push_back(Lookup->second);
	}
	else
	{
		unsigned int Index = (unsigned int)Vertices[Name].size();
		IndexMap[InVertex] = Index;
		Indices[Name].push_back(Index);
		Vertices[Name].push_back(InVertex);
	}
}

FbxAMatrix FbxLoader::GetGeometryTransformation(FbxNode* Node)
{
	const FbxVector4 Translation = Node->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 Rotation = Node->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxVector4 Scaling = Node->GetGeometricScaling(FbxNode::eSourcePivot);

	return FbxAMatrix(Translation, Rotation, Scaling);
}

void FbxLoader::GetPosition(FbxMesh* Mesh, int Index, XMFLOAT3& Data)
{
	const FbxVector4& ControlPoint = Mesh->GetControlPointAt(Index);
	Data = MathHelper::Fbx4ToXM3(ControlPoint);
}

void FbxLoader::GetNormal(FbxMesh* Mesh, int PolyIndex, int VertexIndex, XMFLOAT3& Data)
{
	FbxVector4 Normal;
	bool bResult = Mesh->GetPolygonVertexNormal(PolyIndex, VertexIndex, Normal);
	Data = bResult ? MathHelper::Fbx4ToXM3(Normal) : XMFLOAT3(0.0f, 1.0f, 0.0f);
}

void FbxLoader::GetTexCoord(FbxMesh* Mesh, int PolyIndex, int VertexIndex, XMFLOAT2& Data)
{
	FbxStringList UVNames;
	Mesh->GetUVSetNames(UVNames);

	if (0 == UVNames.GetCount())
	{
		Data = XMFLOAT2(0.0f, 0.0f);
		return;
	}

	FbxVector2 TexCoord;
	bool bUnmapped;
	bool bResult = Mesh->GetPolygonVertexUV(PolyIndex, VertexIndex, UVNames[0], TexCoord, bUnmapped);
	Data = (bResult) ? MathHelper::Fbx2ToXM2(TexCoord) : XMFLOAT2(0.0f, 0.0f);
	Data.y *= -1.0f;
}

const std::vector<Vertex>& FbxLoader::GetVertices(const std::string& Name) const
{
	return Vertices.at(Name);
}

const std::vector<uint16_t>& FbxLoader::GetIndices(const std::string& Name) const
{
	return Indices.at(Name);
}

const std::vector<Texture*> FbxLoader::GetTextures(const std::string& Name) const
{
	std::vector<Texture*> TexList;
	for (int i = 0; i < Textures.at(Name).size(); i++)
	{
		TexList.push_back(Textures.at(Name)[i].get());
	}
	return TexList;
}

const std::vector<Material*> FbxLoader::GetMaterials(const std::string& Name) const
{
	std::vector<Material*> MatList;
	for (int i = 0; i < Materials.at(Name).size(); i++)
	{
		MatList.push_back(Materials.at(Name)[i].get());
	}
	return MatList;
}

const std::vector<XMMATRIX>& FbxLoader::GetBoneOffsets(const std::string& Name) const
{
	return BoneOffsets.at(Name);
}

const std::vector<XMMATRIX>& FbxLoader::GetToRootTransforms(const std::string& Name) const
{
	return ToRootTransforms.at(Name);
}

const int FbxLoader::GetBoneCount(const std::string& Name) const
{
	if (BoneCounts.find(Name) != BoneCounts.end())
	{
		return BoneCounts.at(Name);
	}

	return 0;
}
