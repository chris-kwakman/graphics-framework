#version 420 core

#extension GL_ARB_explicit_uniform_location : enable

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;

layout(location = 0) uniform mat4 u_mvp;

out vec2 f_uv;

void main()
{
	gl_Position = u_mvp * vec4(v_pos.xyz,1.0f);
}