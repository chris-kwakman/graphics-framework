#version 420 core

uniform ubo_fogcam
{
    // Do not reorder
    mat4 u_fog_cam_inv_vp;
    vec3 u_fog_cam_view_dir;
    float u_fog_cam_near;
    vec3 u_fog_cam_world_pos;
    float u_fog_cam_far;
    float u_layer_linearity;
	uint u_layer_count;
};
uniform ubo_camera_data
{
	mat4 u_cam_inv_vp;
	mat4 u_cam_v;
	mat4 u_cam_p;
	vec2 u_cam_viewport_size;
	float u_cam_near;
	float u_cam_far;
	vec3 u_cam_view_dir;
};
const int CSM_PARTITION_COUNT = 3;
uniform ubo_csm_data
{
	mat4	u_csm_vp[CSM_PARTITION_COUNT];
	vec3	u_world_light_dir;
	float	u_csm_shadow_bias[CSM_PARTITION_COUNT]; 
	float	u_csm_clipspace_end[CSM_PARTITION_COUNT];
	float	u_csm_clipspace_blend_start[CSM_PARTITION_COUNT-1];
	float	u_csm_shadow_intensity;
	vec3	u_csm_light_color;
	uint	u_csm_pcf_neighbour_count;
};
uniform sampler2D u_sampler_shadow_map[CSM_PARTITION_COUNT];

uniform vec3 u_ambient_color;
uniform bool u_csm_render_cascades = false;
uniform sampler2D u_sampler_depth;
uniform sampler2D u_sampler_base_color;
uniform sampler2D u_sampler_shadow;
uniform sampler2D u_sampler_ao;
uniform sampler3D u_sampler_volumetric_fog;

in vec2 f_uv;

layout(location = 0) out vec4 out_color;

float linearize_value(float t, float a, float b, float linearity)
{  
    return (1.0 - linearity) * (a * pow(b / a, t)) + linearity * (a + t * (b-a));
}

float get_normalized_fog_depth(float _positive_view_depth)
{
	return linearize_value(_positive_view_depth / u_fog_cam_far, u_fog_cam_near, u_fog_cam_far, u_layer_linearity) / u_fog_cam_far;
}
	
void main()
{
	float sunlight_factor = texture2D(u_sampler_shadow, f_uv).r;
	float ao_factor = texture2D(u_sampler_ao, f_uv).r;

	vec3 texture_color = texture(u_sampler_base_color, f_uv).rgb;

	vec3 frag_color = vec3((u_ambient_color * ao_factor + sunlight_factor * u_csm_light_color) * texture_color);

	const float ndc_z = 2*texture2D(u_sampler_depth, f_uv).r - 1;
	const vec3 ndc = vec3((f_uv * 2) - 1, ndc_z);

	vec4 world_pos = u_cam_inv_vp * vec4(ndc,1);
	world_pos /= world_pos.w;
	
	vec4 view_pos = u_cam_v * world_pos;
	float fog_normalized_depth = get_normalized_fog_depth(-view_pos.z);
	vec4 volfog_value = texture(u_sampler_volumetric_fog, vec3(f_uv.xy, fog_normalized_depth));
	vec3 fog_inscattering = volfog_value.rgb;
	float fog_transmittance = volfog_value.a;

	if(u_csm_render_cascades)
	{
		vec3 frag_mult = vec3(0);
		for(int i = 0; i < CSM_PARTITION_COUNT; ++i)
		{
			if(ndc_z <= u_csm_clipspace_end[i])
			{
				frag_mult[i] = 1;
				break;
			}
		}
		frag_color *= frag_mult;
	}

	vec3 pixel_color = frag_color * fog_transmittance + fog_inscattering;
	out_color += vec4(pixel_color,1);

	//out_color = vec4(world_pos.xyz,1);

	//out_color += vec4(frag_color,1);
}