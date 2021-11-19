#version 420

uniform sampler2D u_sampler_ao;
uniform sampler2D u_sampler_depth;
uniform bool u_bool_blur_horizontal;

uniform float u_sigma;

layout(location = 0) out float out_ao;

in vec2 f_uv;

// Using weights from https://learnopengl.com/Advanced-Lighting/Bloom
const float u_blur_weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

const float pi = 3.1415926;

void main()
{
	// Create gaussian blurring kernel for depth
	float intensity_weights[5];
	const float clamped_sigma = clamp(u_sigma,0,1);
	const float frac_front = 1.0 / (sqrt(2*pi) * clamped_sigma);
	const float frac_exp = - 1.0 / (2*clamped_sigma*clamped_sigma);
	for(int i = 0; i < 5; ++i)
		intensity_weights[i] = frac_front * exp(float(i) * frac_exp);

	const float depth_p = texture(u_sampler_depth, f_uv).r;
	const vec2 texel_offset = 1.0f / vec2(textureSize(u_sampler_ao, 0));
	float result = texture(u_sampler_ao, f_uv).r * u_blur_weights[0];

	// Avoid branching by multipying offset by blur direction
	const vec2 vec_blur_direction = vec2(float(int(u_bool_blur_horizontal)), float(int(!u_bool_blur_horizontal)));
	for(int i = 1; i < 5; ++i)
	{
		const vec2 texel_1 = f_uv + texel_offset * vec_blur_direction * i;
		const vec2 texel_2 = f_uv - texel_offset * vec_blur_direction * i;
		const float depth_q_1 = texture(u_sampler_depth, texel_1).r;
		const float depth_q_2 = texture(u_sampler_depth, texel_2).r;
		const float depth_diff_1 = abs(depth_p - depth_q_1);
		const float depth_diff_2 = abs(depth_p - depth_q_2);
		result += texture(u_sampler_ao, texel_1).r * u_blur_weights[i];
		result += texture(u_sampler_ao, texel_2).r * u_blur_weights[i];
	}

	out_ao = result;
}