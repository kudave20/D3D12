#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include <vector>
#include <memory>

class GameObject;
class Camera;

class IGraphics
{
public:
	virtual ~IGraphics();

public:
	virtual bool Init(Camera* InCamera, std::vector<GameObject*> InGameObjects) = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;
	virtual void OnResize() = 0;

protected:
	std::vector<GameObject*> GameObjects;
};