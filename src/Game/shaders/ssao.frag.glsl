#version 450

layout(push_constant) uniform uniforms_block
{
	vec4 view_matrix_values; // value 0, 5, 10, 14
	vec4 radius; // .x - radius, in world space
};

layout(binding= 0) uniform sampler2D tex;
layout(binding= 1) uniform sampler2D random_vectors_tex;

layout(location= 0) in noperspective vec2 f_tex_coord;

layout(location= 0) out float color;

void main()
{
	int random_vectors_tex_y=
		(int(gl_FragCoord.x) & 3) |
		((int(gl_FragCoord.y) & 3) << 2);

	float fragment_depth= texture(tex, f_tex_coord).x;
	float w= view_matrix_values.w / (fragment_depth - view_matrix_values.z);

	vec2 screen_coord= f_tex_coord * 2.0 - vec2(1.0, 1.0); // in range [-1;+1]
	vec3 world_pos= vec3(w * screen_coord / view_matrix_values.xy, w);

	float occlusion_factor= 0.0;
	const int iterations= 64;
	for(int i= 0; i < iterations; ++i)
	{
		vec3 delta_vec= radius.x * texelFetch(random_vectors_tex, ivec2(i, random_vectors_tex_y), 0).xyz;
		vec3 sample_world_pos= world_pos + delta_vec;

		vec2 sample_screen_pos= vec2(sample_world_pos.xy * view_matrix_values.xy) / sample_world_pos.z;
		float sample_depth= view_matrix_values.z + view_matrix_values.w / sample_world_pos.z;
		vec2 sample_tex_coord= sample_screen_pos * 0.5 + vec2(0.5, 0.5);

		float actual_sample_depth= texture(tex, sample_tex_coord).x;
		occlusion_factor+= step(sample_depth, actual_sample_depth);
	}

	color= occlusion_factor / float(iterations);
}
