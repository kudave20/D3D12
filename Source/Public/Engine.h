#pragma once

#include <Windows.h>
#include <memory>

class WindowManager;
class GameTimer;
class DX12;

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
	bool InitGraphics();
	void InitTimer();

private:
	static Engine* GEngine;

	std::unique_ptr<WindowManager> WindowMgr;

	std::unique_ptr<DX12> Graphics;

	std::unique_ptr<GameTimer> Timer;

	bool bEnginePaused = false;

};
