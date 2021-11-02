#version 420

in vec3 f_pos;
in vec3 f_normal;

uniform sampler2D	u_sampler_depth;
uniform sampler2D	u_sampler_base_color;
uniform sampler2D	u_sampler_metallic_roughness;
uniform sampler2D	u_sampler_normal;

uniform mat4 u_p_inv;

uniform uvec2 u_viewport_size;
uniform float u_camera_near;
uniform float u_camera_far;
uniform vec3 u_light_view_pos;
uniform float u_light_radius;
uniform vec3 u_light_color;

uniform float u_shininess_mult_factor;
uniform vec3 u_bloom_treshhold;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec3 out_luminance; // Used for bloom

vec3 get_view_pos_from_ndc_xy_and_depth(vec2 ndc_xy, float depth)
{
	float ndc_depth = depth * 2 - 1;
	vec4 viewspace_pos = u_p_inv * vec4(ndc_xy, ndc_depth,1);
	viewspace_pos /= viewspace_pos.w;
	return viewspace_pos.xyz;
}

void main()
{
	// Get position of fragment in UV coordinates in framebuffer.
	vec2 framebuffer_uv = vec2(gl_FragCoord.xy) / vec2(u_viewport_size);

	float fb_depth = texture(u_sampler_depth, framebuffer_uv).r;

	vec3 fb_color = texture(u_sampler_base_color, framebuffer_uv).rgb;
	vec3 fb_normal = 2.0f * texture(u_sampler_normal, framebuffer_uv).xyz - 1.0f;
	float fb_shininess = max(texture(u_sampler_metallic_roughness, framebuffer_uv).g, 0.01);
	float fb_specular = texture(u_sampler_metallic_roughness, framebuffer_uv).b;

	vec2 ndc_xy = framebuffer_uv * 2.0 - 1.0;
	vec3 frag_view_pos = get_view_pos_from_ndc_xy_and_depth(ndc_xy, fb_depth);

	vec3 light_dir = u_light_view_pos - frag_view_pos;
	float light_dir_length = length(light_dir);
	light_dir /= light_dir_length;

	float lambertian = max(dot(fb_normal, light_dir), 0.0);
	
	vec3 view_dir = normalize(-frag_view_pos); // Camera position is (0,0,0) in camera space.
	vec3 halfway_dir = normalize(light_dir + view_dir);
	float spec_angle = max(dot(fb_normal, halfway_dir), 0.0);
	float spec_intensity = pow(spec_angle, fb_shininess * u_shininess_mult_factor);
	

	float attenuation = 1 - min(1, light_dir_length / u_light_radius * 2);
	vec3 color_linear = attenuation * u_light_color * (
		 fb_color * lambertian + spec_intensity * vec3(fb_specular)
	);

	float luminance = dot(color_linear, u_bloom_treshhold);

	if(luminance > 0.4)
		out_luminance += vec3(color_linear);
	else
		out_luminance += vec3(0);

	out_color += color_linear;
}