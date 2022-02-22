#version 420 core

uniform vec4 u_base_color_factor = vec4(1,1,1,0.75);
uniform int u_highlight_index = 5;

layout(location = 0) out float fb_depth;
layout(location = 1) out vec4 fb_base_color;

const vec3 face_highlight_color = vec3(1,0,0);

flat in float f_index;

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
	vec4 frag_color = (u_highlight_index == int(f_index)) ? vec4(face_highlight_color, u_base_color_factor.a) : u_base_color_factor;

	fb_depth = 0.0f;
	fb_base_color = frag_color.rgba;

	if(int(f_index * 5) != u_highlight_index)
	{
		float random = mod(rand(vec2(f_index,f_index)), 1.0);
		fb_base_color.rgb *= 0.5 * random + 1.0 * (1 - random);
	}


	//fb_metallic_roughness = texture(u_sampler_metallic_roughness, f_uv_1).rgb;

}