# graphics-framework
 CS562 and CS460 project

### How to use program:

# Camera controls

W/S - Move along forward / backward vector in camera space.
A/D - Move along left / right vector projected onto horizontal world plane.
Q/E - Move along up / down vector in world space.
Shift - Increase movement speed.
Left / Right Arrow  - Rotate camera around yaw.
Up / Down Arrow - Rotate camera around pitch.
Left-mouse drag - Rotate camera.

# Miscellaneous Controls

F5 - Reload shaders.

### Important parts of code:

Engine/Graphics/manager - Stores graphics resources (i.e. textures, buffers, etc...)
Engine/Graphics/light	- Handles lighting part of pipeline
Sandbox/Sandbox			- User-defined application code

### Known issues:

- Application loads in hard-coded sponza.gltf and Sphere.gltf directly from data. Does not take into account first command line argument
- No way to quit program using keyboard buttons.
- Refreshing scene not implemented yet.
- No way to see individual textures corresponding to render pipeline stages yet.
- Lights can only be created, not destroyed.
- Lights are not animated yet.