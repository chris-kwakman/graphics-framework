#version 420 core

#extension GL_ARB_explicit_uniform_location : enable

layout(location = 0) uniform sampler2D u_sampler_base_color;
layout(location = 1) uniform sampler2D u_sampler_metallic_roughness;
layout(location = 2) uniform sampler2D u_sampler_normal;
layout(location = 10) uniform float	u_alpha_cutoff;

in vec3 f_pos;
in vec3 f_normal;
in vec4 f_tangent;
in vec2 f_uv_1;

out vec4 out_color;

void main()
{
	vec4 frag_color = texture(u_sampler_base_color, f_uv_1);
	//frag_color = vec4(texture(u_sampler_metallic_roughness, f_uv_1).rg, 0.0, 1.0);
	//frag_color = vec4(texture(u_sampler_normal, f_uv_1).rgb, 1.0);

	//frag_color = vec4(f_uv_1.x, f_uv_1.y, 0.0, 1.0);
	//frag_color = vec4(f_normal.x, f_normal.y, -f_normal.z, 1.0f);
	//frag_color = vec4(f_tangent.x, f_tangent.y, f_tangent.z, 1.0f);

	if(frag_color.a < u_alpha_cutoff)
		discard;
	out_color = frag_color;
}