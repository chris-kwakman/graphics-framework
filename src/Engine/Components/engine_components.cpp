#include "engine_components.h"

#include <Engine/Utils/singleton.h>

#include "Renderable.h"
#include "Transform.h"

#define INIT_MANAGER(TCompManager) Singleton<TCompManager>().Initialize()
#define SHUTDOWN_MANAGER(TCompManager) Singleton<TCompManager>().Clear()

void Component::InitializeEngineComponentManagers()
{
	INIT_MANAGER(TransformManager);
	INIT_MANAGER(RenderableManager);
}

void Component::ShutdownEngineComponentManagers()
{
	SHUTDOWN_MANAGER(TransformManager);
	SHUTDOWN_MANAGER(RenderableManager);
}
