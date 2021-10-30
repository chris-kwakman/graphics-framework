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

## Known issues:

- ImGuizmo refuses to take mouse input for some indiscernible reason.
- Picking via raycasting is not implemented. However, entities can be selected via the scene graph.
- Cannot import GLTF models with animations but no skins (animations rely on skin with current implementation).
- Cannot import GLTF models that exceed a certain amount of texture coordinate sets per vertex
- Did not implement curve interpolation system or Bezier curves.
- No headers on files as of yet.
- Only using a single scene is currently implemented (though switching between multiple should be trivial to implement with my implementation).
- Issue with `mixamo_walking` since the model uses two skins with the same joints. Not sure if I should handle this.
- Some models like BrainStem are crashing when importing due to memory being overwritten.

## Time of implementation
Like 20 hours

## Time of testing
2 hours

## Comments