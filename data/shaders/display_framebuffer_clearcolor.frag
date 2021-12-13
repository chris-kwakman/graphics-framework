#version 420 core

uniform sampler2D u_sampler_depth;
in vec2 f_uv;

out vec4 out_color;

void main()
{
	if(texture(u_sampler_depth, f_uv).r < 1.0f)
		out_color = vec4(0.0f,0.0f,0.0f,0.0f);	
	else
		discard;
}