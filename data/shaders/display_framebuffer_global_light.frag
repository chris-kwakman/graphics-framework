#version 420 core

const unsigned int CSM_PARTITION_COUNT = 3;

uniform sampler2D u_sampler_depth;
uniform sampler2D u_sampler_base_color;
uniform sampler2D u_sampler_shadow;
uniform sampler2D u_sampler_ao;

uniform vec3 u_ambient_color;
uniform vec3 u_sunlight_color = vec3(1);
uniform bool u_csm_render_cascades = false;
uniform float u_csm_cascade_ndc_end[3];

in vec2 f_uv;

layout(location = 0) out vec4 out_color;

void main()
{
	float sunlight_factor = texture2D(u_sampler_shadow, f_uv).r;
	float ao_factor = texture2D(u_sampler_ao, f_uv).r;

	vec3 texture_color = texture(u_sampler_base_color, f_uv).rgb;

	vec3 frag_color = vec3((u_ambient_color * ao_factor + sunlight_factor * u_sunlight_color) * texture_color);

	if(u_csm_render_cascades)
	{
		float ndc_z = 2*texture2D(u_sampler_depth, f_uv).r - 1;
		vec3 frag_mult = vec3(0);
		for(int i = 0; i < CSM_PARTITION_COUNT; ++i)
		{
			if(ndc_z <= u_csm_cascade_ndc_end[i])
			{
				frag_mult[i] = 1;
				break;
			}
		}
		frag_color *= frag_mult;
	}

	//frag_color *= texture2D(u_sampler_shadow, vec2(0)).r;

	//vec4 frag_color = vec4(texture(u_sampler_metallic_roughness, f_uv).rg, 0.0, 1.0);
	//vec4 frag_color = vec4(texture(u_sampler_normal, f_uv).rgb, 1.0);

	out_color += vec4(frag_color,1);
}