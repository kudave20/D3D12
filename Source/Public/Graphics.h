#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

class IGraphics
{
public:
	virtual bool Init() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;
	virtual void OnResize() = 0;
};