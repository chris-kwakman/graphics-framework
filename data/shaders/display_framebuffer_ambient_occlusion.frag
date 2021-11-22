#version 420 core

struct AO_Config
{
	float radius;
	float angle_bias;
	float attenuation_scale;
	float ao_scale;
	int sample_directions;
	int sample_steps;
};

const float pi = 3.1415926535897932384626433832795;

in vec2 f_uv;

uniform sampler2D u_sampler_depth;
uniform mat4 u_p;
uniform mat4 u_p_inv;
uniform AO_Config u_ao;

layout(location = 0) out float out_ao;

// Random float between 0 and ~1
float rand(vec2 _input)
{
	// Random function blatantly ripped from Stack Overflow page
	// https://stackoverflow.com/questions/4200224/random-noise-functions-for-glsl
	return fract(sin(dot(_input, vec2(12.9898, 78.233))) * 43758.5453);
}

float get_ndc_depth(vec2 _uv)
{
	return (texture(u_sampler_depth, _uv).r * 2) - 1;
}

vec3 get_viewspace_pos(vec2 _uv)
{
	vec4 ndc = vec4((_uv*2)-1, get_ndc_depth(_uv), 1);
	vec4 view_pos = u_p_inv * ndc;
	return view_pos.xyz / view_pos.w;
}

float square_distance(vec3 a, vec3 b)
{
	float dx = a.x-b.x;
	float dy = a.y-b.y;
	float dz = a.z-b.z;
	return (dx*dx)+(dy*dy)+(dz*dz);
}

void main()
{
	// Extract viewspace position from UV.
	const vec3 view_pos = get_viewspace_pos(f_uv);

	// Compute initial random angle from viewspace horizontal axis.
	const float angle_0 = rand(view_pos.xy) * 2 * pi;
	const float dir_angle_delta = (2*pi) / float(u_ao.sample_directions);
	const float step_delta = u_ao.radius / float(u_ao.sample_steps);

	const vec3 vec_dFdx = dFdx(view_pos.xyz);
	const vec3 vec_dFdy = dFdy(view_pos.xyz);

	float accum_ao = 0.0;
	for(int i = 0; i < u_ao.sample_directions; ++i)
	{
		const float sample_dir_angle = angle_0 + float(i)*dir_angle_delta;
		// Linear combination values for rotations in screenspace stored in vector
		const vec2 rotation_lc = vec2(cos(sample_dir_angle), sin(sample_dir_angle));
		
		vec3 view_sample_dir_tangent = vec_dFdx * rotation_lc.x + vec_dFdy * rotation_lc.y;
		// Make sure tangent points towards negative Z (i.e. away from camera).
		view_sample_dir_tangent *= -(view_sample_dir_tangent.z / abs(view_sample_dir_tangent.z));
		const float angle_tangent = atan(view_sample_dir_tangent.z, length(view_sample_dir_tangent.xy));
		const float sin_angle_tangent = sin(angle_tangent + u_ao.angle_bias);

		vec3 min_depth_view_sample_pos = view_pos;

		for(int j = 1; j <= u_ao.sample_steps; ++j)
		{
			// Advance step across XY plane in view space.
			vec4 view_sample_pos = vec4(view_pos + view_sample_dir_tangent * step_delta * float(j), 1);
			// Retrieve view_sample_pos depth at current XY position in NDC using depth texture.
			vec4 ndc_sample_pos = u_p * view_sample_pos;
			ndc_sample_pos /= ndc_sample_pos.w;
			ndc_sample_pos.z = get_ndc_depth((ndc_sample_pos.xy + 1)/2);
			view_sample_pos = u_p_inv * ndc_sample_pos;
			view_sample_pos /= view_sample_pos.w;
			
			if(distance(view_sample_pos.xyz, view_pos) > u_ao.radius)
				continue;

			// Keep track of view sample position that is closest to camera.
			if(view_sample_pos.z > min_depth_view_sample_pos.z)
				min_depth_view_sample_pos = view_sample_pos.xyz;
		}

		vec3 H = min_depth_view_sample_pos - view_pos;
		if(H == vec3(0))
			continue;
		
		const float length_H = length(H);
		H = normalize(H);
		const float angle_h = atan(H.z, length(H.xy));
		const float sin_angle_h = sin(angle_h);

		// Compute linear attenuation function
		const float normalized_radius = length_H / u_ao.radius;
		const float w = max(0, 1 - u_ao.attenuation_scale * normalized_radius * normalized_radius);
		const float ao = sin_angle_h - sin_angle_tangent;
		accum_ao += u_ao.ao_scale * ao * w;
	}
	accum_ao /= u_ao.sample_directions;
	out_ao = 1 - accum_ao;
}