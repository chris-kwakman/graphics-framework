#version 420 core
#extension GL_ARB_explicit_uniform_location : enable

const int NUM_CASCADES = 3;
struct CascadingShadowMapData
{
	mat4 vp[NUM_CASCADES];
	float shadow_bias[NUM_CASCADES];
	sampler2D shadow_map[NUM_CASCADES];
	float clipspace_end[NUM_CASCADES];
	float clipspace_blend_start[NUM_CASCADES-1];
};

struct CameraData
{
	mat4 inv_vp;
	vec2 viewport_size;
	float near;
	float far;
};

layout(location = 0) uniform sampler2D u_sampler_depth;
layout(location = 1) uniform sampler2D u_sampler_normal;

layout(location = 2) uniform float u_shadow_intensity = 0.5f;
layout(location = 3) uniform unsigned int u_pcf_neighbour_count = 2;
layout(location = 4) uniform vec3 u_light_dir;
layout(location = 5) uniform CameraData u_camera_data;
layout(location = 9) uniform CascadingShadowMapData u_csm_data;

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
	float depth = light_ndc.z * 0.5 + 0.5; // Linear depth [0,1]
	vec2 uv = light_ndc.xy * 0.5 + 0.5;
	vec2 uv_step = 1.0 / textureSize(u_csm_data.shadow_map[cascade_index], 0).xy;
	float bias = u_csm_data.shadow_bias[cascade_index];

	float shadow = 0.0;
	uint texel_count = (2*u_pcf_neighbour_count+1)*(2*u_pcf_neighbour_count+1);
	for(int i = -int(u_pcf_neighbour_count); i <= int(u_pcf_neighbour_count); ++i)
	{
		for(int j = -int(u_pcf_neighbour_count); j <= int(u_pcf_neighbour_count); ++j)
		{
			float depth_value = texture(u_csm_data.shadow_map[cascade_index], uv + uv_step * vec2(i,j)).r;
			shadow += (depth_value < (depth - bias)) 
				? (1-u_shadow_intensity) 
				: 1;
		}
	}
	return shadow / texel_count;
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
	//vec3 fb_normal = 2.0f * texture(u_sampler_normal, f_uv).xyz - 1.0f;
	//if(dot(fb_normal, u_light_dir) > 0)
	//{
	//	out_color = 0;//1-u_shadow_intensity;
	//	return;
	//}

	// Find cascade that texel belongs to
	for(unsigned int cascade = 0; cascade < NUM_CASCADES; ++cascade)
	{
		if(ndc_z <= u_csm_data.clipspace_end[cascade])
		{
			shadow_factor = calculate_shadow_factor(cascade, light_space_pos[cascade]);
			if((cascade < NUM_CASCADES-1) && ndc_z > u_csm_data.clipspace_blend_start[cascade])
			{
				float shadow_factor_next = calculate_shadow_factor(cascade+1, light_space_pos[cascade+1]);
			
				float blendspace_distance = u_csm_data.clipspace_end[cascade] - u_csm_data.clipspace_blend_start[cascade];
				float frac = (ndc_z - u_csm_data.clipspace_blend_start[cascade]) / blendspace_distance;
			
				shadow_factor = frac * shadow_factor_next + (1-frac)*shadow_factor;
			}
			break;
		}
	}
	out_color = shadow_factor;
}