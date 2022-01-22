#version 420 core

#extension GL_ARB_explicit_uniform_location : enable

layout(location = 0) in vec2 v_pos;
layout(location = 1) in vec2 v_uv;

out vec2 f_uv;

const vec2 screenTrianglePos[3] = vec2[](
	vec2(-1,-1),vec2(3,-1),vec2(-1,3)
);
const vec2 screenTriangleUV[3] = vec2[] (
	vec2(0,0), vec2(2,0), vec2(0,2)
);

void main()
{
	gl_Position = vec4(screenTrianglePos[gl_VertexID].xy, 0, 1);
	f_uv = screenTriangleUV[gl_VertexID];
}