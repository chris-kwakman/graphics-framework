#include "EngineCompManager.h"

#include <Engine/Utils/singleton.h>

#include "Renderable.h"
#include "Transform.h"
#include "Camera.h"
#include "Light.h"
#include "Nameable.h"

void Component::InitializeEngineComponentManagers()
{
#define DECLARE_MANAGER(TCompManager) Singleton<TCompManager>().Initialize();
#include "EngineComponents.txt"	
#undef DECLARE_MANAGER
}

void Component::ShutdownEngineComponentManagers()
{
#define DECLARE_MANAGER(TCompManager) Singleton<TCompManager>().Clear();
#include "EngineComponents.txt"
#undef DECLARE_MANAGER;
}
