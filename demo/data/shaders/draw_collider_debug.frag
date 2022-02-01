#version 420 core

uniform vec4 u_base_color_factor;
uniform unsigned short u_highlight_face_index;

layout(location = 0) out float fb_depth;
layout(location = 1) out vec3 fb_base_color;
layout(location = 2) out vec3 fb_metallic_roughness;
layout(location = 3) out vec3 fb_normal;

in flat unsigned short f_face_index;

in vec3 f_normal;
in vec3 f_tangent;

const vec3 face_highlight_color = vec3(1,0,0);

void main()
{
	vec4 frag_color = (u_highlight_face_index == f_face_index) ? vec4(face_highlight_color, u_base_color_factor.a) : u_base_color_factor;

	fb_depth = 0.0f;
	fb_base_color = frag_color.rgb;
	//fb_metallic_roughness = texture(u_sampler_metallic_roughness, f_uv_1).rgb;

}