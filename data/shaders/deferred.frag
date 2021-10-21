#version 420 core

#extension GL_ARB_explicit_uniform_location : enable

layout(location = 0) uniform sampler2D u_sampler_base_color;
layout(location = 1) uniform sampler2D u_sampler_metallic_roughness;
layout(location = 2) uniform sampler2D u_sampler_normal;
layout(location = 3) uniform vec4 u_base_color_factor;
layout(location = 10) uniform float	u_alpha_cutoff;

layout(location = 0) out float fb_depth;
layout(location = 1) out vec3 fb_base_color;
layout(location = 2) out vec3 fb_metallic_roughness;
layout(location = 3) out vec3 fb_normal;

in vec3 f_pos;
in vec2 f_uv_1;
in mat3 f_vTBN;

in vec3 f_normal;
in vec3 f_tangent;

void main()
{
	vec4 frag_color = u_base_color_factor * texture(u_sampler_base_color, f_uv_1);
	if(frag_color.a < u_alpha_cutoff)
		discard;

	fb_depth = 0.0f;
	fb_base_color = frag_color.rgb;
	fb_metallic_roughness = texture(u_sampler_metallic_roughness, f_uv_1).rgb;
	

	// Create TBN matrix using Gramm-Schmidt method to re-orthoganalize normal / tangent vectors
	vec3 normal = normalize(f_normal);
	vec3 tangent = normalize(f_tangent);
	tangent = normalize(tangent - dot(tangent, normal) * normal);
	vec3 bitangent = cross(tangent, normal);
	mat3 TBN = mat3(tangent, bitangent, normal);

	// Convert normals in texture from [0,1] range to [-1,1] range.
	fb_normal = normalize(texture(u_sampler_normal, f_uv_1).xyz * vec3(2) - vec3(1));
	fb_normal = normalize(TBN * fb_normal);
	fb_normal = (fb_normal + vec3(1)) * vec3(0.5);

}