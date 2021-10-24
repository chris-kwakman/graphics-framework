#version 420 core

#extension GL_ARB_explicit_uniform_location : enable

layout(location = 0) attribute in vec3 v_pos;
layout(location = 1) attribute in vec3 v_normal;
layout(location = 2) attribute in vec4 v_tangent;
layout(location = 3) attribute in vec2 v_uv_1;
layout(location = 5) attribute in vec4 v_joints;
layout(location = 6) attribute in vec4 v_weights;

layout(location = 5) uniform mat4 u_mvp;
layout(location = 6) uniform mat4 u_mv_t_inv;
layout(location = 7) uniform mat4 u_p_inv;
layout(location = 8) uniform mat4 u_v;
layout(location = 9) uniform mat4 u_mv;

layout(location = 16) uniform mat4 u_joint_matrices[128];

out vec2 f_uv_1;
out mat3 f_vTBN; // Matrix that brings normal map vectors to view space.

out vec3 f_normal;
out vec3 f_tangent;

void main()
{
	mat4 skin_matrix = 
		u_joint_matrices[int(v_joints.x)] * v_weights.x +
		u_joint_matrices[int(v_joints.y)] * v_weights.y +
		u_joint_matrices[int(v_joints.z)] * v_weights.z +
		u_joint_matrices[int(v_joints.w)] * v_weights.w;

	gl_Position = u_mvp * skin_matrix * vec4(v_pos.xyz,1.0f);

	f_uv_1 = v_uv_1;

	f_normal = vec3(u_mv_t_inv * vec4(v_normal,0));
	f_tangent = vec3(u_mv * vec4(v_tangent.xyz,0));

}