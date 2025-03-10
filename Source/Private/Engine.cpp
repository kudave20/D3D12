#include "Engine.h"
#include "Framework/GameTimer.h"
#include "Framework/Camera.h"
#include "FbxLoader.h"
#include "Window.h"
#include "DX12.h"
#include "Landscape.h"
#include "Rock.h"
#include "Dummy.h"
#include <cassert>
#include <windowsx.h>

Engine* Engine::GEngine = nullptr;

Engine::Engine()
{
	assert(GEngine == nullptr);
	GEngine = this;
}

Engine::~Engine()
{
	Graphics->FlushCommandQueue();
}

Engine* Engine::GetEngine()
{
	return GEngine;
}

LRESULT Engine::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			bEnginePaused = true;
			GameTimer::Get()->Stop();
		}
		else
		{
			bEnginePaused = false;
			GameTimer::Get()->Start();
		}
		return 0;

	case WM_SIZE:
		WindowMgr->AddWidth(hwnd, LOWORD(lParam));
		WindowMgr->AddHeight(hwnd, HIWORD(lParam));

		if (wParam == SIZE_MINIMIZED)
		{
			bEnginePaused = true;
			WindowMgr->SetMinimizedByHWND(hwnd, true);
			WindowMgr->SetMaximizedByHWND(hwnd, false);
		}
		else if (wParam == SIZE_MAXIMIZED)
		{
			bEnginePaused = false;
			WindowMgr->SetMinimizedByHWND(hwnd, false);
			WindowMgr->SetMaximizedByHWND(hwnd, true);
			WindowMgr->OnResize();
		}
		else if (wParam == SIZE_RESTORED)
		{
			if (WindowMgr->GetMinimizedByHWND(hwnd))
			{
				bEnginePaused = false;
				WindowMgr->SetMinimizedByHWND(hwnd, false);
				WindowMgr->OnResize();
			}
			else if (WindowMgr->GetMaximizedByHWND(hwnd))
			{
				bEnginePaused = false;
				WindowMgr->SetMaximizedByHWND(hwnd, false);
				WindowMgr->OnResize();
			}
			else if (false == WindowMgr->GetResizingByHWND(hwnd))
			{
				WindowMgr->OnResize();
			}
		}
		return 0;

	case WM_ENTERSIZEMOVE:
		bEnginePaused = true;
		WindowMgr->SetResizingByHWND(hwnd, true);
		GameTimer::Get()->Stop();
		return 0;

	case WM_EXITSIZEMOVE:
		bEnginePaused = false;
		WindowMgr->SetResizingByHWND(hwnd, false);
		GameTimer::Get()->Start();
		WindowMgr->OnResize();
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_MENUCHAR:
		return MAKELRESULT(0, MNC_CLOSE);

	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		WindowMgr->OnPointerDown(hwnd, wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		WindowMgr->OnPointerUp(hwnd, wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_MOUSEMOVE:
		WindowMgr->OnPointerMove(hwnd, wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		else if ((int)wParam == VK_F2)
		{
			//Graphics->Toggle4xMsaaState();
		}

		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Engine::Init()
{
	InitTimer();
	InitLoader();
	InitCamera();

	if (false == InitWindow())
	{
		return false;
	}

	InitGameObjects();

	if (false == InitGraphics())
	{
		return false;
	}

	WindowMgr->SetGraphics(Graphics.get());

	return true;
}

bool Engine::InitWindow()
{
	WindowMgr = std::make_unique<WindowManager>();

	if (false == WindowMgr->Init(MainCamera.get()))
	{
		return false;
	}

	return true;
}

void Engine::InitGameObjects()
{
	/*std::unique_ptr<Landscape> LandScapeGO = std::make_unique<Landscape>(MainCamera.get());
	GameObjects.push_back(std::move(LandScapeGO));*/

	/*std::unique_ptr<Rock> RockGO = std::make_unique<Rock>(MainCamera.get());
	RockGO->Scale(2.0f, 2.0f, 2.0f);
	GameObjects.push_back(std::move(RockGO));*/

	std::unique_ptr<Dummy> DummyGO = std::make_unique<Dummy>(MainCamera.get());
	DummyGO->Scale(0.1f, 0.1f, 0.1f);
	GameObjects.push_back(std::move(DummyGO));
}

bool Engine::InitGraphics()
{
	Graphics = std::make_unique<DX12>();

	std::vector<GameObject*> GameObjectPtrs;
	for (int i = 0; i < GameObjects.size(); i++)
	{
		if (GameObjects[i])
		{
			GameObjectPtrs.push_back(GameObjects[i].get());
		}
	}

	if (false == Graphics->Init(MainCamera.get(), GameObjectPtrs))
	{
		return false;
	}

	return true;
}

void Engine::InitTimer()
{
	Timer = std::make_unique<GameTimer>();
	Timer->Init();
}

void Engine::InitLoader()
{
	Loader = std::make_unique<FbxLoader>();
	Loader->Init();
}

void Engine::InitCamera()
{
	MainCamera = std::make_unique<Camera>();
}

int Engine::Tick()
{
	MSG msg = { 0 };

	GameTimer::Get()->Reset();

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			GameTimer::Get()->Tick();

			if (false == bEnginePaused)
			{
				WindowMgr->CalculateFrameStats();
				Graphics->Update();
				Graphics->Draw();
			}
			else
			{
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}
