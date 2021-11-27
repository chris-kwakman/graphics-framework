#include "SandboxCompManager.h"

#include <Engine/Utils/singleton.h>



void Component::InitializeSandboxComponentManagers()
{
#define DECLARE_MANAGER(TCompManager) Singleton<TCompManager>().Initialize();
#include "SandboxComponents.txt"	
#undef DECLARE_MANAGER
}

void Component::ShutdownSandboxComponentManagers()
{
#define DECLARE_MANAGER(TCompManager) Singleton<TCompManager>().Clear();
#include "SandboxComponents.txt"
#undef DECLARE_MANAGER;
}
