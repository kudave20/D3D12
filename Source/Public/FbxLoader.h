#pragma once

#include <fbxsdk.h>
#include <DirectXMath.h>
#include <vector>
#include "FrameResource.h"

using namespace DirectX;

class FbxLoader
{
public:
	FbxLoader();
	~FbxLoader();

public:
	static FbxLoader* Get();

public:
	void Init();

public:
	bool Load(const char* FilePath);

private:
	void LoadTexture(const char* FilePath, FbxScene* Scene);
	void LoadMaterial(FbxScene* Scene);
	bool LoadMesh(FbxScene* Scene);

private:
	std::wstring ConvertToTextureName(const char* FilePath, const std::string& Name);
	bool FindMesh(FbxNode* Node, FbxMesh*& OutMesh);
	void ProcessPolygon(FbxMesh* Mesh);
	void InsertVertex(const Vertex& InVertex);

	void GetPosition(FbxMesh* Mesh, int Index, XMFLOAT3& Data);
	void GetNormal(FbxMesh* Mesh, int PolyIndex, int VertexIndex, XMFLOAT3& Data);
	void GetTexCoord(FbxMesh* Mesh, int PolyIndex, int VertexIndex, XMFLOAT2& Data);

public:
	const std::vector<Vertex>& GetVertices() const;
	const std::vector<uint16_t>& GetIndices() const;
	const std::vector<Texture*> GetTextures() const;
	const std::vector<Material*> GetMaterials() const;

private:
	XMFLOAT3 Fbx4ToXM3(const FbxVector4& Fbx) const;
	XMFLOAT2 Fbx2ToXM2(const FbxVector2& Fbx) const;

private:
	FbxManager* Manager = nullptr;
	FbxIOSettings* IOSettings = nullptr;
	FbxImporter* Importer = nullptr;

private:
	static FbxLoader* Loader;

private:
	std::vector<std::unique_ptr<Texture>> Textures;
	std::vector<std::unique_ptr<Material>> Materials;

	std::vector<Vertex> Vertices;
	std::vector<uint16_t> Indices;
	std::unordered_map<Vertex, uint16_t> IndexMap;
};

