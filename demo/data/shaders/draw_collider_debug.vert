#version 420 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in unsigned short v_face_index;

uniform mat4 u_mvp;

out flat unsigned short f_face_index;

void main()
{
	gl_Position = u_mvp * vec4(v_pos.xyz,1.0f);
	f_face_index = v_face_index;
}