#include "Window.h"
#include "Engine.h"
#include "DX12.h"
#include "Framework/GameTimer.h"
#include "Framework/Camera.h"
#include <string>
#include <cassert>

using namespace std;

WindowManager* WindowManager::Manager = nullptr;

WindowManager::WindowManager()
{
	assert(Manager == nullptr);
	Manager = this;
}

WindowManager::~WindowManager()
{
	for (auto&& Window : Windows)
	{
		Window.reset();
	}
	Windows.clear();
}

WindowManager* WindowManager::Get()
{
	return Manager;
}

bool WindowManager::Init(Camera* InCamera)
{
	for (int Index = 0; Index < WindowsCount; Index++)
	{
		std::unique_ptr<Window> NewWindow = std::make_unique<Window>();
		if (false == NewWindow->Init(InitialWidth, InitialHeight, InCamera))
		{
			return false;
		}

		Windows.push_back(std::move(NewWindow));
	}

	return true;
}

void WindowManager::OnResize()
{
	if (nullptr != Graphics)
	{
		Graphics->OnResize();
	}
}

void WindowManager::OnPointerDown(HWND HWnd, WPARAM State, int X, int Y)
{
	Window* CurrentWindow = GetWindowByHWND(HWnd);
	if (nullptr != CurrentWindow)
	{
		CurrentWindow->OnPointerDown(State, X, Y);
	}
}

void WindowManager::OnPointerMove(HWND HWnd, WPARAM State, int X, int Y)
{
	Window* CurrentWindow = GetWindowByHWND(HWnd);
	if (nullptr != CurrentWindow)
	{
		CurrentWindow->OnPointerMove(State, X, Y);
	}
}

void WindowManager::OnPointerUp(HWND HWnd, WPARAM State, int X, int Y)
{
	Window* CurrentWindow = GetWindowByHWND(HWnd);
	if (nullptr != CurrentWindow)
	{
		CurrentWindow->OnPointerUp(State, X, Y);
	}
}

void WindowManager::CalculateFrameStats()
{
	static int FrameCount = 0;
	static float ElapsedTime = 0.0f;

	FrameCount++;

	if ((GameTimer::Get()->TotalTime() - ElapsedTime) >= 1.0f)
	{
		float Fps = (float)FrameCount;
		float Mspf = 1000.0f / Fps;

		wstring FpsStr = to_wstring(Fps);
		wstring MspfStr = to_wstring(Mspf);

		wstring WindowText = wstring(GetFirstWindow()->GetName()) +
			L"    fps: " + FpsStr +
			L"   mspf: " + MspfStr;

		SetWindowText(GetFirstWindow()->GetWindow(), WindowText.c_str());

		FrameCount = 0;
		ElapsedTime += 1.0f;
	}
}

void WindowManager::SetGraphics(DX12* InGraphics)
{
	Graphics = InGraphics;

	for (auto&& Window : Windows)
	{
		Window->SetGraphics(InGraphics);
	}
}

void WindowManager::AddWidth(HWND HWnd, int InWidth)
{
	Window* CurrentWindow = GetWindowByHWND(HWnd);
	if (nullptr != CurrentWindow)
	{
		CurrentWindow->AddWidth(InWidth);
	}
}

void WindowManager::AddHeight(HWND HWnd, int InHeight)
{
	Window* CurrentWindow = GetWindowByHWND(HWnd);
	if (nullptr != CurrentWindow)
	{
		CurrentWindow->AddHeight(InHeight);
	}
}

void WindowManager::SetMinimizedByHWND(HWND HWnd, bool InMinimized)
{
	Window* CurrentWindow = GetWindowByHWND(HWnd);
	if (nullptr != CurrentWindow)
	{
		CurrentWindow->SetMinimized(InMinimized);
	}
}

void WindowManager::SetMaximizedByHWND(HWND HWnd, bool InMaximized)
{
	Window* CurrentWindow = GetWindowByHWND(HWnd);
	if (nullptr != CurrentWindow)
	{
		CurrentWindow->SetMaximized(InMaximized);
	}
}

void WindowManager::SetResizingByHWND(HWND HWnd, bool InResizing)
{
	Window* CurrentWindow = GetWindowByHWND(HWnd);
	if (nullptr != CurrentWindow)
	{
		CurrentWindow->SetResizing(InResizing);
	}
}

Window* WindowManager::GetFirstWindow()
{
	return Windows.size() > 0 ? Windows[0].get() : nullptr;
}

DX12* WindowManager::GetGraphics() const
{
	return Graphics;
}

bool WindowManager::GetMinimizedByHWND(HWND HWnd) const
{
	Window* CurrentWindow = GetWindowByHWND(HWnd);
	if (nullptr != CurrentWindow)
	{
		CurrentWindow->GetMinimized();
	}

	return nullptr;
}

bool WindowManager::GetMaximizedByHWND(HWND HWnd) const
{
	Window* CurrentWindow = GetWindowByHWND(HWnd);
	if (nullptr != CurrentWindow)
	{
		CurrentWindow->GetMaximized();
	}

	return nullptr;
}

bool WindowManager::GetResizingByHWND(HWND HWnd) const
{
	Window* CurrentWindow = GetWindowByHWND(HWnd);
	if (nullptr != CurrentWindow)
	{
		CurrentWindow->GetResizing();
	}

	return nullptr;
}

Window* WindowManager::GetWindowByHWND(const HWND& HWnd) const
{
	for (auto&& Window : Windows)
	{
		if (HWnd == Window->GetWindow())
		{
			return Window.get();
		}
	}

	return nullptr;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return Engine::GetEngine()->WindowProc(hwnd, msg, wParam, lParam);
}

HINSTANCE Window::HInst = nullptr;

Window::~Window()
{
	DestroyWindow(HWnd);
	UnregisterClass(WndName.c_str(), GetInstance());
}

void Window::OnPointerDown(WPARAM State, int X, int Y)
{
	SetCapture(HWnd);
}

void Window::OnPointerMove(WPARAM State, int X, int Y)
{
	if ((State & MK_LBUTTON) != 0)
	{
		float Dx = XMConvertToRadians(0.25f * static_cast<float>(X - LastMousePos.x));
		float Dy = XMConvertToRadians(0.25f * static_cast<float>(Y - LastMousePos.y));

		MainCamera->Pitch(Dy);
		MainCamera->RotateY(Dx);
	}

	LastMousePos.x = X;
	LastMousePos.y = Y;
}

void Window::OnPointerUp(WPARAM state, int x, int y)
{
	ReleaseCapture();
}

bool Window::Init(int InWidth, int InHeight, Camera* InCamera)
{
	HInst = GetModuleHandle(nullptr);

	WNDCLASS Wc;
	ZeroMemory(&Wc, sizeof(Wc));
	Wc.style = CS_HREDRAW | CS_VREDRAW;
	Wc.lpfnWndProc = MainWndProc;
	Wc.cbClsExtra = 0;
	Wc.cbWndExtra = 0;
	Wc.hInstance = HInst;
	Wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	Wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	Wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	Wc.lpszMenuName = nullptr;
	Wc.lpszClassName = WndName.c_str();

	if (false == RegisterClass(&Wc))
	{
		return false;
	}

	RECT R = { 0, 0, InWidth, InHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	Width = R.right - R.left;
	Height = R.bottom - R.top;

	HWnd = CreateWindow(
		WndName.c_str(),
		WndName.c_str(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		Width, Height,
		nullptr,
		nullptr,
		HInst,
		nullptr);

	if (false == HWnd)
	{
		MessageBox(0, L"CreateWindow 실패!", 0, 0);
		return false;
	}

	ShowWindow(HWnd, SW_SHOW);
	UpdateWindow(HWnd);

	MainCamera = InCamera;

	return true;
}

void Window::SetMinimized(bool InMinimized)
{
	bMinimized = InMinimized;
}

void Window::SetMaximized(bool InMaximized)
{
	bMaximized = InMaximized;
}

void Window::SetResizing(bool InResizing)
{
	bResizing = InResizing;
}

void Window::AddWidth(bool InWidth)
{
	Width += InWidth;
}

void Window::AddHeight(bool InHeight)
{
	Height += InHeight;
}

void Window::SetGraphics(DX12* InGraphics)
{
	Graphics = InGraphics;
}

bool Window::GetMinimized() const
{
	return bMinimized;
}

bool Window::GetMaximized() const
{
	return bMaximized;
}

bool Window::GetResizing() const
{
	return bResizing;
}

int Window::GetWidth() const
{
	return Width;
}

int Window::GetHeight() const
{
	return Height;
}

HINSTANCE Window::GetInstance()
{
	return HInst;
}

const HWND Window::GetWindow() const
{
	return HWnd;
}

const wstring Window::GetName() const
{
	return WndName;
}

void Window::SetName(const wstring InName)
{
	WndName = InName;
}
