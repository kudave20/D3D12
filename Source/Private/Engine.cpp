#include "Engine.h"
#include "Framework/GameTimer.h"
#include "Window.h"
#include "DX12.h"
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
			// Graphics->Toggle4xMsaaState();
		}

		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Engine::Init()
{
	InitTimer();

	if (false == InitWindow())
	{
		return false;
	}

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

	if (false == WindowMgr->Init())
	{
		return false;
	}

	return true;
}

bool Engine::InitGraphics()
{
	Graphics = std::make_unique<DX12>();

	if (false == Graphics->Init())
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
