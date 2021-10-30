#version 420 core

#extension GL_ARB_explicit_uniform_location : enable

layout(location = 0) uniform vec4 u_base_color_factor;

layout(location = 0) out float fb_depth;
layout(location = 1) out vec3 fb_base_color;

in vec3 f_pos;
in vec2 f_uv_1;
in mat3 f_vTBN;

in vec3 f_normal;
in vec3 f_tangent;

void main()
{
	fb_base_color = u_base_color_factor.rgb;
}