#version 420 core

#extension GL_ARB_explicit_uniform_location : enable

layout(location = 0) uniform sampler2D u_sampler_scene;
layout(location = 1) uniform sampler2D u_sampler_bloom;
layout(location = 2) uniform sampler2D u_sampler_depth;

layout(location = 10) uniform float u_exposure;
layout(location = 11) uniform float u_gamma;

in vec2 f_uv;

layout(location =0) out vec4 out_color;

void main()
{
	vec3 scene_color = texture(u_sampler_scene, f_uv).rgb;
	vec3 bloom_color = texture(u_sampler_bloom, f_uv).rgb;
	scene_color += bloom_color;
	
	// Tone mapping
	vec3 result = vec3(1) - exp(-scene_color * u_exposure);

	// Gamma correction
	result = pow(result, vec3(1.0f / u_gamma));

	out_color = vec4(result,1);

	gl_FragDepth = texture(u_sampler_depth, f_uv).r;
}