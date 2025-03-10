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
	bool Load(const char* FilePath, const std::string& Name);

private:
	void LoadTexture(const char* FilePath, FbxScene* Scene, const std::string& Name);
	void LoadMaterial(FbxScene* Scene, const std::string& Name);
	FbxMesh* LoadMesh(FbxScene* Scene, const std::string& Name);
	void LoadAnimation(FbxScene* Scene, const std::string& Name, FbxMesh* Mesh);

private:
	std::wstring ConvertToTextureName(const char* FilePath, const std::string& Name);
	bool FindMesh(FbxNode* Node, FbxMesh*& OutMesh);
	void FindAnimation(FbxScene* Scene, FbxMesh* Mesh, const FbxLongLong& Time, const std::string& Name);
	void ProcessPolygon(FbxMesh* Mesh, const std::string& Name);
	void InsertVertex(const Vertex& InVertex, const std::string& Name);
	FbxAMatrix GetGeometryTransformation(FbxNode* Node);

	void GetPosition(FbxMesh* Mesh, int Index, XMFLOAT3& Data);
	void GetNormal(FbxMesh* Mesh, int PolyIndex, int VertexIndex, XMFLOAT3& Data);
	void GetTexCoord(FbxMesh* Mesh, int PolyIndex, int VertexIndex, XMFLOAT2& Data);

public:
	const std::vector<Vertex>& GetVertices(const std::string& Name) const;
	const std::vector<uint16_t>& GetIndices(const std::string& Name) const;
	const std::vector<Texture*> GetTextures(const std::string& Name) const;
	const std::vector<Material*> GetMaterials(const std::string& Name) const;
	const std::vector<XMMATRIX>& GetBoneOffsets(const std::string& Name) const;
	const std::vector<XMMATRIX>& GetToRootTransforms(const std::string& Name) const;
	const int GetBoneCount(const std::string& Name) const;

private:
	FbxManager* Manager = nullptr;
	FbxIOSettings* IOSettings = nullptr;
	FbxImporter* Importer = nullptr;

private:
	static FbxLoader* Loader;

private:
	std::unordered_map<std::string, std::vector<std::unique_ptr<Texture>>> Textures;
	std::unordered_map<std::string, std::vector<std::unique_ptr<Material>>> Materials;

	std::unordered_map<std::string, std::vector<Vertex>> Vertices;
	std::unordered_map<std::string, std::vector<uint16_t>> Indices;
	std::unordered_map<Vertex, uint16_t> IndexMap;

	std::unordered_map<std::string, std::vector<XMMATRIX>> BoneOffsets;
	std::unordered_map<std::string, std::vector<XMMATRIX>> ToRootTransforms;

	std::unordered_map<std::string, int> BoneCounts;
};

