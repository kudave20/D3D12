#pragma once

#include <Windows.h>
#include <memory>
#include <vector>

class WindowManager;
class GameTimer;
class FbxLoader;
class DX12;
class GameObject;
class Camera;

class Engine
{
public:
	Engine();
	Engine(const Engine& Rhs) = delete;
	Engine& operator=(const Engine& Rhs) = delete;
	~Engine();

public:
	static Engine* GetEngine();

public:
	virtual LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
	bool Init();
	int Tick();

private:
	bool InitWindow();
	void InitGameObjects();
	bool InitGraphics();
	void InitTimer();
	void InitLoader();
	void InitCamera();

private:
	static Engine* GEngine;

	std::unique_ptr<WindowManager> WindowMgr;

	std::vector<std::unique_ptr<GameObject>> GameObjects;

	std::unique_ptr<FbxLoader> Loader;

	std::unique_ptr<DX12> Graphics;

	std::unique_ptr<GameTimer> Timer;

	std::unique_ptr<Camera> MainCamera;

	bool bEnginePaused = false;

};
