#version 420 core

#extension GL_ARB_explicit_uniform_location : enable

layout(location = 0) uniform sampler2D u_sampler_base_color;
layout(location = 1) uniform sampler2D u_sampler_metallic_roughness;
layout(location = 2) uniform sampler2D u_sampler_normal;

layout(location = 10) uniform vec3 u_ambient_color;

in vec2 f_uv;

out vec4 out_color;

void main()
{
	vec4 frag_color = vec4(u_ambient_color * texture(u_sampler_base_color, f_uv).rgb, 1);
	//vec4 frag_color = vec4(texture(u_sampler_metallic_roughness, f_uv).rg, 0.0, 1.0);
	//vec4 frag_color = vec4(texture(u_sampler_normal, f_uv).rgb, 1.0);

	out_color = frag_color;
}