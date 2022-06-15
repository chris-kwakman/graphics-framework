# Description

This framework contains code for providing functionality for a entity-component system, graphics and 3D rigidbody physics.
This framework takes a data-oriented design, or DOD, approach with its component system, where all components or the same type are stored mostly contiguously, and processed sequentially.
In theory this would allow for performance benefits and trivialize processing the component data in parallel.

# Instructions

This project uses CMake and VCPKG for its buildsystem.

If Visual Studio is used, specify a CMake toolchain file if a default cannot be found on your machine.
This toolchain file should be located in `[VCPKG directory]/scripts/buildsystems/vcpkg.cmake`

Executables and binaries will be installed in their corresponding folders when built.

# Source Code Locations

`demo` folder contains source code for demo and rendering.
`engine` folder contains source code for entity component system (ECS), graphics utilities, physics and miscellaneous.
`test` folder contains source code for unit tests.