#version 420

uniform sampler2D u_sampler_ao;
uniform sampler2D u_sampler_depth;
uniform bool u_bool_blur_horizontal;

uniform float u_sigma;
uniform float u_camera_near;
uniform float u_camera_far;

layout(location = 0) out float out_ao;

in vec2 f_uv;

// Using weights from https://learnopengl.com/Advanced-Lighting/Bloom
const float u_blur_weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

const float pi = 3.1415926;

float get_linear_depth(float _ndc_depth) {
    return (2.0 * u_camera_near) / (u_camera_far + u_camera_near - _ndc_depth * (u_camera_far - u_camera_near));
}

void main()
{
	// Create gaussian blurring kernel for depth
	float intensity_weights[5];
	const float clamped_sigma = max(u_sigma, 0.1);
	const float frac_front = 1.0 / (sqrt(2*pi) * clamped_sigma);
	const float frac_exp = - 1.0 / (2*clamped_sigma*clamped_sigma);

	const float depth_p = texture(u_sampler_depth, f_uv).r;
	const vec2 texel_offset = 1.0f / vec2(textureSize(u_sampler_ao, 0));
	float result = texture(u_sampler_ao, f_uv).r * u_blur_weights[0];

	// Avoid branching by multipying offset by blur direction
	const vec2 vec_blur_direction = vec2(float(int(u_bool_blur_horizontal)), float(int(!u_bool_blur_horizontal)));
	float w_p = u_blur_weights[0];
	for(int i = 1; i < 5; ++i)
	{
		const vec2 texel_1 = f_uv + texel_offset * vec_blur_direction * i;
		const vec2 texel_2 = f_uv - texel_offset * vec_blur_direction * i;
		const float depth_q_1 = get_linear_depth(texture(u_sampler_depth, texel_1).r );
		const float depth_q_2 = get_linear_depth(texture(u_sampler_depth, texel_2).r );
		const float depth_diff_1 = depth_p - depth_q_1;
		const float depth_diff_2 = depth_p - depth_q_2;

		// Compute gaussian multiplier
		const float gauss_mult_1 = u_blur_weights[i] * frac_front * exp(depth_diff_1 * depth_diff_1 * frac_exp);
		const float gauss_mult_2 = u_blur_weights[i] * frac_front * exp(depth_diff_2 * depth_diff_2 * frac_exp);

		result += gauss_mult_1 * texture(u_sampler_ao, texel_1).r + gauss_mult_2 * texture(u_sampler_ao, texel_2).r;
		w_p += (gauss_mult_1 + gauss_mult_2);

		//result += gauss_s_q_1 + gauss_s_q_2;
	}

	out_ao = result / w_p;
	//out_ao = result;
}