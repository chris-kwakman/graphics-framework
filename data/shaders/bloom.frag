#version 420

#extension GL_ARB_explicit_uniform_location : enable

layout(location = 0) uniform sampler2D u_sampler_bloom_input;
layout(location = 1) uniform bool u_bool_blur_horizontal;

layout(location = 0) out vec3 out_bloom;

in vec2 f_uv;

// Using weights from https://learnopengl.com/Advanced-Lighting/Bloom
float u_blur_weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{

	vec2 texel_offset = 1.0f / vec2(textureSize(u_sampler_bloom_input, 0));
	vec3 result = texture(u_sampler_bloom_input, f_uv).rgb * u_blur_weights[0];
	
	// Avoid branching by multipying offset by blur direction
	vec2 vec_blur_direction = vec2(float(int(u_bool_blur_horizontal)), float(int(!u_bool_blur_horizontal)));
	for(int i = 1; i < 5; ++i)
	{
		result += texture(u_sampler_bloom_input, f_uv + texel_offset * vec_blur_direction * i).rgb * u_blur_weights[i];
		result += texture(u_sampler_bloom_input, f_uv - texel_offset * vec_blur_direction * i).rgb * u_blur_weights[i];
	}

	out_bloom = result;
}