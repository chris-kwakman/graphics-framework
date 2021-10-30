#include "EngineCompManager.h"

#include <Engine/Utils/singleton.h>

#include "Transform.h"
#include "Renderable.h"
#include "SkeletonAnimator.h"
#include "Camera.h"
#include "Light.h"
#include "Nameable.h"
#include "CurveInterpolator.h"

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
