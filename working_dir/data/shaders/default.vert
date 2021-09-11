#version 420 core

#extension GL_ARB_explicit_uniform_location : enable

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec4 v_tangent;
layout(location = 3) in vec2 v_uv_1;

layout(location = 5) uniform mat4 u_mvp;
layout(location = 6) uniform mat4 u_mv_t_inv;

out vec3 f_normal;
out vec3 f_tangent;
out vec2 f_uv_1;

void main()
{
	gl_Position = u_mvp * vec4(v_pos.xyz + vec3(0,0,10),1.0f);
	f_normal = vec3(u_mvp * vec4(v_normal.xyz, 0.0f));
	f_tangent = vec3(u_mv_t_inv * v_tangent);
	f_uv_1 = v_uv_1;
}