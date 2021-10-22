#version 420 core

#extension GL_ARB_explicit_uniform_location : enable

layout(location = 0) uniform sampler2D u_sampler_base_color;
layout(location = 1) uniform sampler2D u_sampler_shadow;

layout(location = 10) uniform vec3 u_ambient_color;
layout(location = 11) uniform vec3 u_sunlight_color = vec3(1);

in vec2 f_uv;

layout(location = 0) out vec4 out_color;

void main()
{
	float sunlight_factor = texture2D(u_sampler_shadow, f_uv).r;

	vec3 texture_color = texture(u_sampler_base_color, f_uv).rgb;

	vec3 frag_color = vec3((u_ambient_color + sunlight_factor * u_sunlight_color) * texture_color);

	//frag_color *= texture2D(u_sampler_shadow, vec2(0)).r;

	//vec4 frag_color = vec4(texture(u_sampler_metallic_roughness, f_uv).rg, 0.0, 1.0);
	//vec4 frag_color = vec4(texture(u_sampler_normal, f_uv).rgb, 1.0);

	out_color += vec4(frag_color,1);
}