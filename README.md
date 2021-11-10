# graphics-framework
 CS562 and CS460 project

Name: Christian Thomas Maria Kwakman
Login: c.kwakman

### Additional Notes

## How to use program:

# Camera controls

W/S - Move along forward / backward vector in camera space.
A/D - Move along left / right vector projected onto horizontal world plane.
Q/E - Move along up / down vector in world space.
Shift - Increase movement speed.
Left / Right Arrow  - Rotate camera around yaw.
Up / Down Arrow - Rotate camera around pitch.
Left-mouse drag - Rotate camera.
V - Reset camera transform to starting transform

# Miscellaneous Controls

F5 - Reload shaders.
CTRL+SHIFT+R - Reload resources (also resets scene graph)
CTRL+Q - Quit Program

CTRL+1 - Load animation scene
CTRL+2 - Load curves scene

Drag / Drop GLTF files to load them as models.
Drag / Drop models within editor graphics resources tab into an entity to create that model's hierarchy.
Drag / Drop animation resource into SkeletonAnimator to set animation.

## Important Files
- Sandbox/Sandbox.cpp - Contains graphics pipeline
- Engine/Components/Renderable.h/cpp - Contains renderable components including Decal.

## Known Issues:

- Angle treshhold for decals does not seem to work right.
- Some decals have white outlines. Blending issue?