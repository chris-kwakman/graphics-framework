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

Drag / Drop GLTF files to load them as models.
Drag / Drop models within editor graphics resources tab into an entity to create that model's hierarchy.
Drag / Drop animation resource into SkeletonAnimator to set animation.
Drag / Drop entities from scene graph into certain boxes in components(i.e. Curve Follower) to assign their components to other components.

CTRL+WASD - Move player character in demo

## Known Issues:

- No easy way to sync up animations right now, however, time warps can be manually set for each animation.
- Some animation glitches when time reaches the end for certain animations. Not sure what causes it, should investigate more deeply when I have the time.

## Additional Notes:

I have set up a demo with a interactive player character.
Holding LCTRL + WASD will move the player character along the XZ-plane relative to the view direction of the camera.
The player character's animation will be blended dependent on its current velocity and what direction the player chamera is relative to the player character. This means that the player character will idle, walk or run depending on its speed, and will always try to look at the camera.

I have also implemented a fairly user-friendly Blend Tree editor, which can be accessed in the demo by going to any entity with a `SkeletonAnimator` component (i.e. the entity in the MIXAMO hierarchy that represents the hips) and clicking on the `Edit Blend Tree` button. Three types of nodes exist: Leaf nodes, 1D blend nodes and 2D blend nodes. Animations can only be added to Leaf nodes in the Blend Tree. These animations can be set by drag-dropping an animation resource from `Graphics Resources/Animations` into the appropriate slot in the Blend Tree editor. 1D and 2D blend nodes can have all other kinds of nodes as children that they can interpolate between. 1D blend nodes also have the option of setting a blend mask to determine which joints will be picked from the left and right node's pose. 2D blend nodes do not have blend masks.

All blend trees can be serialized and deserialized through the blend tree editor as well, which can be accessed in the top-left of the Blend Tree Editor window under `Files`. I haven't tested it, but this will most likely crash the application since the player controller directly modifies the default blend tree parameters through pointers (thus resulting in out-of-bound crash).

# Time of implementation

20 hours

# Time of testing

2 minutes