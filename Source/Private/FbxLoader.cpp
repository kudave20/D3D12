#include "FbxLoader.h"
#include <cassert>

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

	Importer = FbxImporter::Create(Manager, "");
}

bool FbxLoader::Load(const char* FilePath)
{
	if (false == Importer->Initialize(FilePath, -1, Manager->GetIOSettings()))
	{
		return false;
	}

	Textures.clear();
	Materials.clear();
	Vertices.clear();
	Indices.clear();

	FbxScene* Scene = FbxScene::Create(Manager, "My Scene");
	Importer->Import(Scene);
	Importer->Destroy();

	FbxAxisSystem::MayaYUp.ConvertScene(Scene);

	FbxGeometryConverter Converter(Manager);
	Converter.Triangulate(Scene, true);

	LoadTexture(FilePath, Scene);
	LoadMaterial(Scene);
	LoadMesh(Scene);

	return true;
}

void FbxLoader::LoadTexture(const char* FilePath, FbxScene* Scene)
{
	int TextureCount = Scene->GetTextureCount();
	for (int i = 0; i < TextureCount; ++i)
	{
		FbxTexture* Tex = Scene->GetTexture(i);
		std::unique_ptr<Texture> Data = std::make_unique<Texture>();
		Data->Name = Tex->GetName();
		Data->Filename = ConvertToTextureName(FilePath, Data->Name);
		Data->Index = (int)Textures.size();
		Textures.push_back(std::move(Data));
	}
}

void FbxLoader::LoadMaterial(FbxScene* Scene)
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
			Mat->DiffuseSrvHeapIndex = Textures[i]->Index;
		}
		else
		{
			Mat->DiffuseSrvHeapIndex = 0;
		}

		if (ClassID.Is(FbxSurfacePhong::ClassId))
		{
			FbxSurfacePhong* Phong = (FbxSurfacePhong*)(SurfaceMaterial);
			XMFLOAT3 DiffuseColor = Fbx4ToXM3(Phong->Diffuse.Get());

			Mat->DiffuseAlbedo = XMFLOAT4(DiffuseColor.x, DiffuseColor.y, DiffuseColor.z, 1.0f);
			Mat->Name = Phong->GetName();
		}
		else if (ClassID.Is(FbxSurfaceLambert::ClassId))
		{
			FbxSurfaceLambert* Lambert = (FbxSurfaceLambert*)(SurfaceMaterial);
			XMFLOAT3 DiffuseColor = Fbx4ToXM3(Lambert->Diffuse.Get());

			Mat->DiffuseAlbedo = XMFLOAT4(DiffuseColor.x, DiffuseColor.y, DiffuseColor.z, 1.0f);
			Mat->Name = Lambert->GetName();
		}

		Mat->MatCBIndex = (int)Materials.size();

		// FIXME: 하드코딩 없애자
		Mat->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
		Mat->Roughness = 0.3f;

		Materials.push_back(std::move(Mat));
	}
}

bool FbxLoader::LoadMesh(FbxScene* Scene)
{
	FbxNode* RootNode = Scene->GetRootNode();
	FbxMesh* Mesh = nullptr;
	if (false == FindMesh(RootNode, Mesh))
	{
		return false;
	}

	ProcessPolygon(Mesh);

	return true;
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

void FbxLoader::ProcessPolygon(FbxMesh* Mesh)
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

			InsertVertex(Vertex);
		}
	}
}

void FbxLoader::InsertVertex(const Vertex& InVertex)
{
	auto Lookup = IndexMap.find(InVertex);
	if (Lookup != IndexMap.end())
	{
		Indices.push_back(Lookup->second);
	}
	else
	{
		unsigned int Index = (unsigned int)Vertices.size();
		IndexMap[InVertex] = Index;
		Indices.push_back(Index);
		Vertices.push_back(InVertex);
	}
}

void FbxLoader::GetPosition(FbxMesh* Mesh, int Index, XMFLOAT3& Data)
{
	const FbxVector4& ControlPoint = Mesh->GetControlPointAt(Index);
	Data = Fbx4ToXM3(ControlPoint);
}

void FbxLoader::GetNormal(FbxMesh* Mesh, int PolyIndex, int VertexIndex, XMFLOAT3& Data)
{
	FbxVector4 Normal;
	bool bResult = Mesh->GetPolygonVertexNormal(PolyIndex, VertexIndex, Normal);
	Data = bResult ? Fbx4ToXM3(Normal) : XMFLOAT3(0.0f, 1.0f, 0.0f);
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
	Data = (bResult) ? Fbx2ToXM2(TexCoord) : XMFLOAT2(0.0f, 0.0f);
	Data.y *= -1.0f;
}

const std::vector<Vertex>& FbxLoader::GetVertices() const
{
	return Vertices;
}

const std::vector<uint16_t>& FbxLoader::GetIndices() const
{
	return Indices;
}

const std::vector<Texture*> FbxLoader::GetTextures() const
{
	std::vector<Texture*> TexList;
	for (int i = 0; i < Textures.size(); i++)
	{
		TexList.push_back(Textures[i].get());
	}
	return TexList;
}

const std::vector<Material*> FbxLoader::GetMaterials() const
{
	std::vector<Material*> MatList;
	for (int i = 0; i < Materials.size(); i++)
	{
		MatList.push_back(Materials[i].get());
	}
	return MatList;
}

XMFLOAT3 FbxLoader::Fbx4ToXM3(const FbxVector4& Fbx) const
{
	return XMFLOAT3(static_cast<float>(Fbx.mData[0]), static_cast<float>(Fbx.mData[1]), static_cast<float>(Fbx.mData[2]));
}

XMFLOAT2 FbxLoader::Fbx2ToXM2(const FbxVector2& Fbx) const
{
	return XMFLOAT2(static_cast<float>(Fbx.mData[0]), static_cast<float>(Fbx.mData[1]));
}
