# Description

This framework contains code for providing functionality for a entity-component system, graphics and 3D rigidbody physics.
This framework takes a data-oriented design, or DOD, approach with its component system, where all components or the same type are stored mostly contiguously, and processed sequentially.
In theory this would allow for performance benefits and trivialize processing the component data in parallel.

# User Instuctions

- WASD to control camera in scene containing a camera object. +SHIFT to move faster.
- Drag LMB to rotate camera. 
- Menu bar to access different menus.
- Top-left "Scene->Load Scene" menu to open example scenes.
- Hold "F" over rigidbodies and release to apply force in direction of camera.
- Graphics panel has "Debug Entity Colors" button: Enable for nicer visualization when many objects exist.

# Build Instructions

This project uses CMake and VCPKG for its buildsystem.

If Visual Studio is used, specify a CMake toolchain file if a default cannot be found on your machine.
This toolchain file should be located in `[VCPKG directory]/scripts/buildsystems/vcpkg.cmake`

Executables and binaries will be installed in their corresponding folders when built.

# Source Code Locations

`demo` folder contains source code for demo and rendering.
`engine` folder contains source code for entity component system (ECS), graphics utilities, physics and miscellaneous.
`test` folder contains source code for unit tests.


# Retrospective Thoughts (2025)

This framework was originally written between 2021-2022: Since then, my work experience has made clear to me the importance of maintainability and documentation. Looking back to this project with that in mind only reinforces that.

The mindset here is very clearly with feature development in mind first and foremost. Documentation is largely nonexistent, and most of the codebase was clearly hard-coded (with the past justification being "data-oriented design"). The resource manager in particular suffers from this, and is practically an unholy god-object. In hindsight, I would have spent much more time focusing on extendability and abstraction through interfaces. Additionally, I would like to modernize the code to use later C++ features, especially for data that is only used on the CPU. This includes using smart pointers, and using std::variant over unions. 

Furthermore, the bias towards using handles over pointers is also very apparent in this project. I do to this day still believe that although handles provide a lot of inherent benefits, particularly the trivialization of serialization of data structures, it also comes with a lot of downsides. The primary downside being that some level of implementation must be exposed when handle-associated data is accessed outside of the owning system. This also introduces a lot of boilerplate code that would otherwise not be (as) necessary using conventional OOP through pointer access. In hindsight, providing wrappers for handles to associate them to their owning system may have been a venue worth exploring.

Lastly, while this is a "graphics framework", most of the graphics capabilities have fallen to the wayside in favor of showcasing physics capabilities instead. A lot of old features (i.e. loading GLTF scenes) were not maintained as a result, and the full potential of this framework cannot actually be showcased without additional work to bring these features back. In particular, a scene showcasing volumetric fog is missing. Additionally, even though this project claims to use "data-oriented design" for performance reasons, the performance of this project is brought to its figurative knees by the fact that the graphics pipeline does not perform batch rendering. This goes to show that premature optimzation, without proactively performing performance profiling, can be a flawed idea.