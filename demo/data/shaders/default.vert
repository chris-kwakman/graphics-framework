#version 420 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec4 v_tangent;
layout(location = 3) in vec2 v_uv_1;

uniform mat4 u_mvp;
uniform mat4 u_mv_t_inv;
uniform mat4 u_p_inv;
uniform mat4 u_v;
uniform mat4 u_mv;

out vec2 f_uv_1;
out mat3 f_vTBN; // Matrix that brings normal map vectors to view space.

out vec3 f_normal;
out vec3 f_tangent;

void main()
{
	gl_Position = u_mvp * vec4(v_pos.xyz,1.0f);

	f_uv_1 = v_uv_1;

	f_normal = vec3(u_mv_t_inv * vec4(v_normal,0));
	f_tangent = vec3(u_mv * vec4(v_tangent.xyz,0));

}