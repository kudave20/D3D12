#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <memory>

class DX12;
class Camera;

class Window
{
public:
	~Window();

public:
	void OnPointerDown(WPARAM State, int X, int Y);
	void OnPointerMove(WPARAM State, int X, int Y);
	void OnPointerUp(WPARAM State, int X, int Y);

public:
	bool Init(int Width, int Height, Camera* InCamera);

public:
	void SetMinimized(bool InMinimized);
	void SetMaximized(bool InMaximized);
	void SetResizing(bool InResizing);

	void AddWidth(bool InWidth);
	void AddHeight(bool InHeight);

public:
	void SetGraphics(DX12* InGraphics);

public:
	bool GetMinimized() const;
	bool GetMaximized() const;
	bool GetResizing() const;

	int GetWidth() const;
	int GetHeight() const;

public:
	static HINSTANCE GetInstance();
	const HWND GetWindow() const;
	const std::wstring GetName() const;

public:
	void SetName(const std::wstring InName);

private:
	static HINSTANCE HInst;

	HWND HWnd = nullptr;

	DX12* Graphics = nullptr;

	Camera* MainCamera = nullptr;

	std::wstring WndName = L"그래픽스 엔진";

	bool bMinimized = false;
	bool bMaximized = false;
	bool bResizing = false;

	int Width = 0;
	int Height = 0;

	POINT LastMousePos = {};
};

class WindowManager
{
public:
	WindowManager();
	~WindowManager();
	WindowManager(const WindowManager& Rhs) = delete;
	WindowManager& operator=(const WindowManager& Rhs) = delete;

public:
	static WindowManager* Get();

public:
	bool Init(Camera* InCamera);
	void OnResize();

public:
	void OnPointerDown(HWND HWnd, WPARAM State, int X, int Y);
	void OnPointerMove(HWND HWnd, WPARAM State, int X, int Y);
	void OnPointerUp(HWND HWnd, WPARAM State, int X, int Y);

public:
	void CalculateFrameStats();

public:
	void SetGraphics(DX12* InGraphics);

	void AddWidth(HWND HWnd, int InWidth);
	void AddHeight(HWND HWnd, int InHeight);
	void SetMinimizedByHWND(HWND HWnd, bool InMinimized);
	void SetMaximizedByHWND(HWND HWnd, bool InMaximized);
	void SetResizingByHWND(HWND HWnd, bool InResizing);

public:
	Window* GetFirstWindow();

	DX12* GetGraphics() const;

	bool GetMinimizedByHWND(HWND HWnd) const;
	bool GetMaximizedByHWND(HWND HWnd) const;
	bool GetResizingByHWND(HWND HWnd) const;

	int GetElapsedFrame() const;

private:
	Window* GetWindowByHWND(const HWND& HWnd) const;

private:
	static WindowManager* Manager;

private:
	std::vector<std::unique_ptr<Window>> Windows;

	DX12* Graphics = nullptr;

	int FrameCount = 0;
	int ElapsedFrame = 0;

	const int WindowsCount = 1;

	int InitialWidth = 1280;
	int InitialHeight = 720;
};
