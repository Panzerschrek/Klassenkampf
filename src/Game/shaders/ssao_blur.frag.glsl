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
	ivec2 coord= ivec2(gl_FragCoord.xy);

	float frag_depth= texelFetch(depth_tex, coord, 0).x;
	float frag_w= view_matrix_values.w / (frag_depth - view_matrix_values.z);

	float value= 0.0;
	float weights= 0.0;
	float weight_factor= 1.0 / (2.0 * radius.x);
	for(int dx= -1; dx <= 2; ++dx)
	for(int dy= -1; dy <= 2; ++dy)
	{
		ivec2 sample_coord= coord + ivec2(dx, dy);
		float sample_depth= texelFetch(depth_tex, sample_coord, 0).x;
		float sample_w= view_matrix_values.w / (sample_depth - view_matrix_values.z);

		float weight= 1.0 - min(1.0, abs(frag_w - sample_w) * weight_factor);
		value+= weight * texelFetch(ssao_tex, sample_coord, 0).x;
		weights+= weight;
	}

	color= value / weights;
}
