#version 450

layout(push_constant) uniform uniforms_block
{
	vec4 view_matrix_values; // value 0, 5, 10, 14
	vec4 radius; // .x - radius, in world space
};

layout(binding= 0) uniform sampler2D depth_tex;
layout(binding= 1) uniform sampler2D ssao_tex;

layout(location= 0) out float color;

void main()
{
	float value= 0.0;
	ivec2 coord= ivec2(gl_FragCoord.xy);
	for(int dx= -1; dx <= 2; ++dx)
	for(int dy= -1; dy <= 2; ++dy)
	{
		value+= texelFetch(ssao_tex, coord + ivec2(dx, dy), 0).x;
	}

	color= value / 16.0;
}
