#ifndef SANDBOX_H
#define SANDBOX_H

#include "VerletIntegration/verlet_integration.h"

namespace Sandbox
{
	bool Initialize(int argc, char * argv[]);
	void Update();
	void Shutdown();

	extern VerletIntegration::particle_system s_verlet_particle_system;
}

#endif // !SANDBOX_H