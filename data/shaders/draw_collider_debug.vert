#version 420 core

layout(location = 0) in vec3 v_pos;
// Passed to shader as uint16_t, but OpenGL casts it to float automatically??
layout(location = 1) in float v_index;

uniform mat4 u_mvp;

// When rendering convex hull face mesh, this index corresponds to the index of the face in the
// convex hull data structure.
// When rendering convex hull edge mesh, this index corresponds to the index of the edge spanning
// the two vertices of the line currently being drawn.
flat out float f_index;

void main()
{
	f_index = v_index;
	gl_Position = u_mvp * vec4(v_pos.xyz,1.0f);
}