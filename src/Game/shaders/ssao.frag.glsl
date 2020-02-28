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
	// Reconstruct normal, using depth values.
	// use 4 neighbor texels and select delta vector with minimum absolute depth change.
	vec2 inv_tex_size= vec2(1.0, 1.0) / vec2(textureSize(tex, 0));
	float fragment_depth= texture(tex, f_tex_coord).x;
	float depth_x_plus = texture(tex, f_tex_coord + vec2(+inv_tex_size.x, 0)).x;
	float depth_x_minus= texture(tex, f_tex_coord + vec2(-inv_tex_size.x, 0)).x;
	float depth_y_plus = texture(tex, f_tex_coord + vec2(0, +inv_tex_size.y)).x;
	float depth_y_minus= texture(tex, f_tex_coord + vec2(0, -inv_tex_size.y)).x;
	vec2 screen_coord= f_tex_coord * 2.0 - vec2(1.0, 1.0); // in range [-1;+1]
	vec2 screen_coord_x_plus = (f_tex_coord + vec2(+inv_tex_size.x, 0.0)) * 2.0 - vec2(1.0, 1.0);
	vec2 screen_coord_x_minus= (f_tex_coord + vec2(-inv_tex_size.x, 0.0)) * 2.0 - vec2(1.0, 1.0);
	vec2 screen_coord_y_plus = (f_tex_coord + vec2(0.0, +inv_tex_size.y)) * 2.0 - vec2(1.0, 1.0);
	vec2 screen_coord_y_minus= (f_tex_coord + vec2(0.0, -inv_tex_size.y)) * 2.0 - vec2(1.0, 1.0);

	float depth_delta_x_plus = abs(fragment_depth - depth_x_plus );
	float depth_delta_x_minus= abs(fragment_depth - depth_x_minus);
	float depth_delta_y_plus = abs(fragment_depth - depth_y_plus );
	float depth_delta_y_minus= abs(fragment_depth - depth_y_minus);

	float w= view_matrix_values.w / (fragment_depth - view_matrix_values.z);
	float w_x_plus = view_matrix_values.w / (depth_x_plus  - view_matrix_values.z);
	float w_x_minus= view_matrix_values.w / (depth_x_minus - view_matrix_values.z);
	float w_y_plus = view_matrix_values.w / (depth_y_plus  - view_matrix_values.z);
	float w_y_minus= view_matrix_values.w / (depth_y_minus - view_matrix_values.z);

	vec3 world_pos= vec3(w * screen_coord / view_matrix_values.xy, w);
	vec3 world_pos_x_plus = vec3(w_x_plus  * screen_coord_x_plus  / view_matrix_values.xy, w_x_plus );
	vec3 world_pos_x_minus= vec3(w_x_minus * screen_coord_x_minus / view_matrix_values.xy, w_x_minus);
	vec3 world_pos_y_plus = vec3(w_y_plus  * screen_coord_y_plus  / view_matrix_values.xy, w_y_plus );
	vec3 world_pos_y_minus= vec3(w_y_minus * screen_coord_y_minus / view_matrix_values.xy, w_y_minus);

	vec3 vnx= mix(world_pos_x_plus - world_pos, world_pos - world_pos_x_minus, step(depth_delta_x_minus, depth_delta_x_plus));
	vec3 vny= mix(world_pos_y_plus - world_pos, world_pos - world_pos_y_minus, step(depth_delta_y_minus, depth_delta_y_plus));
	vec3 normal= normalize(cross(vny, vnx));

	int random_vectors_tex_y=
		(int(gl_FragCoord.x) & 3) |
		((int(gl_FragCoord.y) & 3) << 2);

	float occlusion_factor= 0.0;
	const int iterations= 64;
	for(int i= 0; i < iterations; ++i)
	{
		vec3 delta_vec= radius.x * texelFetch(random_vectors_tex, ivec2(i, random_vectors_tex_y), 0).xyz;

		// Reflect delta vector against plane of surface, using plane normal.
		float vec_dot= dot(delta_vec, normal);
		delta_vec-= step(vec_dot, 0.0) * 2.0 * vec_dot * normal;

		vec3 sample_world_pos= world_pos + delta_vec;

		vec2 sample_screen_pos= vec2(sample_world_pos.xy * view_matrix_values.xy) / sample_world_pos.z;
		float sample_depth= view_matrix_values.z + view_matrix_values.w / sample_world_pos.z;
		vec2 sample_tex_coord= sample_screen_pos * 0.5 + vec2(0.5, 0.5);

		float actual_sample_depth= texture(tex, sample_tex_coord).x;
		occlusion_factor+= step(sample_depth, actual_sample_depth);
	}

	color= occlusion_factor / float(iterations);
}
