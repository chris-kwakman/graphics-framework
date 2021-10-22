#version 420 core
#extension GL_ARB_explicit_uniform_location : enable

const int NUM_CASCADES = 3;
struct CascadingShadowMapData
{
	mat4 vp[NUM_CASCADES];
	float clipspace_end[NUM_CASCADES];
	sampler2D shadow_map[NUM_CASCADES];
};

struct CameraData
{
	mat4 inv_vp;
	vec2 viewport_size;
	float near;
	float far;
};

layout(location = 0) uniform sampler2D u_sampler_depth;

layout(location = 1) uniform CameraData u_camera_data;
layout(location = 5) uniform CascadingShadowMapData u_csm_data;

in vec2 f_uv;
out float out_color;

float linear_depth_to_ndc_depth(float _linear_depth)
{
	return _linear_depth * 2.0 - 1.0;
}

float get_uv_ndc_depth()
{
	float lin_depth = texture(u_sampler_depth, f_uv).r;
	return linear_depth_to_ndc_depth(lin_depth);
}

vec4 get_uv_world_pos(float _ndc_depth)
{
	vec2 ndc_xy = f_uv * 2 - 1;
	vec4 ndc_pos = vec4(ndc_xy, _ndc_depth, 1.0f);
	vec4 world_pos = u_camera_data.inv_vp * ndc_pos;
	return world_pos / world_pos.w;
}

vec4 get_texel_world_pos()
{
	vec2 ndc_xy = f_uv * 2 - 1;
	vec4 ndc_pos = vec4(ndc_xy, get_uv_ndc_depth(), 1.0f);
	vec4 world_pos = u_camera_data.inv_vp * ndc_pos;
	world_pos /= world_pos.w;
	return world_pos;
}


float calculate_shadow_factor(unsigned int cascade_index, vec4 light_space_pos)
{
	vec3 light_ndc = light_space_pos.xyz / light_space_pos.w;
	vec2 uv = light_ndc.xy * 0.5 + 0.5;
	float depth = light_ndc.z * 0.5 + 0.5; // Linear depth [0,1]
	// Only sample texture at cascade index layer in mip map (cascade index 0 = base level = highest resolution texture).
	float shadow_depth = texture2D(u_csm_data.shadow_map[cascade_index], uv).r;
	if(shadow_depth < depth + 0.00001)
		return 0.5;
	else
		return 1.0;
}

void main()
{
	vec4 light_space_pos[NUM_CASCADES];

	float ndc_z = get_uv_ndc_depth();
	vec4 texel_world_pos = get_texel_world_pos();
	for(unsigned int cascade = 0; cascade < NUM_CASCADES; ++cascade)
	{
		light_space_pos[cascade] = u_csm_data.vp[cascade] * texel_world_pos;
	}

	if(ndc_z == 1.0)
	{
		out_color = 0.0;
		return;
	}

	float shadow_factor = 1;

	// Find cascade that texel belongs to
	for(unsigned int cascade = 0; cascade < 3; ++cascade)
	{
		if(ndc_z <= u_csm_data.clipspace_end[cascade])
		{
			shadow_factor = calculate_shadow_factor(cascade, light_space_pos[cascade]);
			break;
		}
	}
	out_color = shadow_factor;
}