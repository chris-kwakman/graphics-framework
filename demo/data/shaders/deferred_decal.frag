#version 420 core

uniform sampler2D	u_sampler_base_color;
uniform sampler2D	u_sampler_metallic_roughness;
uniform sampler2D	u_sampler_normal;
uniform sampler2D	u_sampler_deferred_depth;
uniform sampler2D	u_sampler_deferred_normal;

uniform vec4		u_base_color_factor;
uniform float		u_alpha_cutoff;
uniform int			u_decal_render_mode;
uniform mat4		u_mvp;
uniform mat4		u_mvp_inv;
uniform mat4		u_p_inv;
uniform mat4		u_mv_t_inv;
uniform uvec2		u_viewport_size;
uniform float		u_decal_angle_treshhold;

layout(location = 0) out vec3 fb_base_color;
layout(location = 1) out vec3 fb_metallic_roughness;
layout(location = 2) out vec3 fb_normal;

in vec2 f_uv_1;
in mat3 f_vTBN;

in vec4 f_pos;
in vec3 f_normal;
in vec3 f_tangent;

vec3 decal_cube_min = vec3(-0.5);
vec3 decal_cube_max = vec3(0.5);

void main()
{

	// Render decal as mask
	if(u_decal_render_mode != 0)
	{
		// Convert fragment position in NDC space to framebuffer UV.
		vec2 fb_uv = gl_FragCoord.xy / u_viewport_size;
		// Get depth of deferred framebuffer fragment behind current cube fragment and convert to ndc pos
		float linear_depth = texture(u_sampler_deferred_depth, fb_uv).r;
		float ndc_z = (linear_depth * 2)-1;
		vec4 scene_ndc_pos = vec4((fb_uv*2)-1, ndc_z, 1);
		// Compute view space position for normal computation later on.
		vec4 view_pos = u_p_inv * vec4(scene_ndc_pos.xyz, 1);
		view_pos /= view_pos.w;
		// Convert scene NDC to model space of cube.
		vec4 scene_model_pos = u_mvp_inv * scene_ndc_pos;
		scene_model_pos /= scene_model_pos.w;

		// Discard fragment if it is not within boundaries of cube model used by decal.
		bvec3 less_than_min = lessThan(scene_model_pos.xyz, decal_cube_min);
		bvec3 greater_than_max = greaterThan(scene_model_pos.xyz, decal_cube_max);
		if(any(less_than_min) || any(greater_than_max))
			discard;

		vec2 model_texture_uv = (scene_model_pos.xy+0.5);
		vec3 model_tex_normal = (texture(u_sampler_normal, model_texture_uv.xy).xyz *2) - 1;
		vec4 frag_color = texture(u_sampler_base_color, model_texture_uv).rgba;
		if(frag_color.a < u_alpha_cutoff)
			discard;

		// Set albedo and metallic roughness values from decal into gbuffer
		fb_base_color = frag_color.rgb;
		fb_metallic_roughness = texture(u_sampler_metallic_roughness, model_texture_uv).rgb;

		// Compute new normal using decal normal texture and geometry normal from dFdx and dFdy.
		vec3 screen_tangent = normalize(dFdx(view_pos.xyz));
		vec3 screen_bitangent = normalize(dFdy(view_pos.xyz));
		vec3 screen_normal = normalize(cross(screen_tangent, screen_bitangent));

		vec4 decal_dir_vec_screen = normalize(u_mv_t_inv * vec4(0,0,-1,0));
		float decal_rel_angle = acos(dot(decal_dir_vec_screen.xyz, screen_normal));

		if(u_decal_render_mode != 1 && (decal_rel_angle > u_decal_angle_treshhold))
			discard;

		mat3 matrix_TBN = mat3(screen_tangent, screen_bitangent, screen_normal);
		fb_normal = (matrix_TBN * model_tex_normal);
		fb_normal = (fb_normal + 1)/2;
		//fb_normal = (scene_ndc_normal.xyz + 1)/4;
		//fb_normal = (scene_ndc_normal + 1) / 2;

		//fb_base_color.rgb = (screen_normal.xyz + 1)/2;
	}
	else
	{
		// Create TBN matrix using Gramm-Schmidt method to re-orthoganalize normal / tangent vectors
		vec3 normal = normalize(f_normal);
		vec3 tangent = normalize(f_tangent);
		tangent = normalize(tangent - dot(tangent, normal) * normal);
		vec3 bitangent = cross(tangent, normal);
		mat3 TBN = mat3(tangent, bitangent, normal);

		vec4 frag_color = u_base_color_factor * texture(u_sampler_base_color, f_uv_1);
		if(frag_color.a < u_alpha_cutoff)
			discard;

		fb_base_color = frag_color.rgb;
		fb_metallic_roughness = texture(u_sampler_metallic_roughness, f_uv_1).rgb;
	
		// Convert normals in texture from [0,1] range to [-1,1] range.
		fb_normal = normalize(texture(u_sampler_normal, f_uv_1).xyz * vec3(2) - vec3(1));
		fb_normal = normalize(TBN * fb_normal);
		fb_normal = (fb_normal + vec3(1)) * vec3(0.5);
	}

}