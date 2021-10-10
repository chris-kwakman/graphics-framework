# graphics-framework
 CS562 and CS460 project

Name: Christian Thomas Maria Kwakman
Login: c.kwakman

### Additional Notes

## Extra Credit implemented:
- Advanced rendering (from CS562)
- Parent / Unparent nodes

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

## Important parts of code:

Engine/Graphics/manager - Stores graphics resources (i.e. textures, buffers, etc...)
Engine/Components/		- Engine components are defined here (Transform, Light, Camera)
Sandbox/Sandbox			- User-defined application code

## Known issues:

- ImGuizmo refuses to take mouse input for some indiscernible reason.
- ImGuizmo crashes in release for some reason.
- Scene graph only shows nodes with entity IDs and not names.
- CTRL+R (resource refresh) not implemented.
- Picking via raycasting is not implemented. However, entities can be selected via the scene graph.
- Infinite grid has issues with depth rendering (overlays ontop of other objects even while trying to respect depth)
- Models are loaded on their side. Probably a scene deserialization issue.

## Time of implementation
Too much time (>20 hours ontop of CS562?)

## Time of testing
1 to 2 hours

## Comments
ImGuizmo is causing issues, the reasons of which I do not know.