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
	// Use simple blur, instead of detph-based, because reading depth image is too expensive.
	ivec2 coord= ivec2(gl_FragCoord.xy);

	float value= 0.0;

	for(int dx= -1; dx <= 2; ++dx)
	for(int dy= -1; dy <= 2; ++dy)
	{
		ivec2 sample_coord= coord + ivec2(dx, dy);
		value+= texelFetch(ssao_tex, sample_coord, 0).x;
	}

	color= value / 16.0;
}
