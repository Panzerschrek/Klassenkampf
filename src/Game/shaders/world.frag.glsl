#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require


struct Light
{
	vec4 pos; // .z contains fade factor for light radius.
	vec4 color;
	vec2 data; // .x contains invert radius
	ivec2 shadowmap_index; // .x - number of cubemap array, .y - layer number
};

layout(set= 0, binding= 0, std430) buffer readonly light_buffer_block
{
	// Use vec4 for fit alignment.
	vec4 ambient_color;
	ivec4 cluster_volume_size;
	vec2 viewport_size;
	vec2 w_convert_values;
	Light lights[];
};

layout(set= 0, binding= 1, std430) buffer readonly cluster_offset_buffer_block
{
	int light_offsets[];
};

layout(set= 0, binding= 2, std430) buffer readonly lights_list_buffer_block
{
	uint8_t light_list[];
};

layout(set= 0, binding= 4) uniform sampler2D ambient_occlusion_image;

layout(set= 0, binding= 5) uniform samplerCubeArrayShadow depth_cubemaps_array[4];

layout(set= 1, binding=  8) uniform sampler2D albedo_tex;
layout(set= 1, binding=  9) uniform sampler2D normals_tex;
layout(set= 1, binding= 10) uniform sampler2D occlusion_tex;

layout(location= 0) in mat3 f_texture_space_mat;
layout(location= 3) in vec2 f_tex_coord;
layout(location= 4) in vec3 f_pos; // World space position.

layout(location = 0) out vec4 out_color;

void main()
{
	// Do mipmapped images fetches before any branching, because branching may break mip calculation.

	vec4 albedo_alpha= texture(albedo_tex, f_tex_coord);
	float occlusion= texture(occlusion_tex, f_tex_coord).r;

	// Reconstruct z, because normal map may not contain it or may be invalud.
	vec2 map_normal_xy= texture(normals_tex, f_tex_coord).xy * 2.0 - vec2(1.0, 1.0);
	vec3 map_normal= vec3(map_normal_xy, sqrt(max(0.0, 1.0 - dot(map_normal_xy, map_normal_xy))));
	vec3 normal_normalized= normalize(f_texture_space_mat * map_normal);

	vec2 frag_coord_normalized= gl_FragCoord.xy / viewport_size;

	vec3 cluster_coord=
		vec3(
			cluster_volume_size.xy * frag_coord_normalized,
			w_convert_values.x * log2(w_convert_values.y * gl_FragCoord.w));
	int offset= int(light_offsets[
		int(cluster_coord.x) +
		int(cluster_coord.y) * cluster_volume_size.x +
		int(cluster_coord.z) * (cluster_volume_size.x * cluster_volume_size.y) ]);

	vec3 l= ambient_color.rgb * (0.5 + 0.5 * occlusion * texture(ambient_occlusion_image, frag_coord_normalized).r);
	int current_light_count= light_list[offset];
	for(int i= 0; i < current_light_count; ++i)
	{
		int light_index= int(light_list[offset + 1 + i]);
		Light light= lights[light_index];
		vec3 vec_to_light= light.pos.xyz - f_pos;
		vec3 vec_to_light_normalized= normalize(vec_to_light);
		float vec_to_light_square_length= dot(vec_to_light, vec_to_light);
		float cos_factor= max(dot(normal_normalized, vec_to_light_normalized), 0.0);
		float fade_factor= max(1.0 / vec_to_light_square_length - light.pos.w, 0.0);
		float normalized_distance_to_light= length(vec_to_light) * light.data.x;

		vec4 shadowmap_coord= vec4(vec_to_light, float(light.shadowmap_index.y));
		float shadow_factor= 1.0;
		if(light.shadowmap_index.x == 0)
			shadow_factor= texture(depth_cubemaps_array[0], shadowmap_coord, normalized_distance_to_light);
		else if(light.shadowmap_index.x == 1)
			shadow_factor= texture(depth_cubemaps_array[1], shadowmap_coord, normalized_distance_to_light);
		else if(light.shadowmap_index.x == 2)
			shadow_factor= texture(depth_cubemaps_array[2], shadowmap_coord, normalized_distance_to_light);
		else if(light.shadowmap_index.x == 3)
			shadow_factor= texture(depth_cubemaps_array[3], shadowmap_coord, normalized_distance_to_light);

		l+= light.color.rgb * (cos_factor * fade_factor * shadow_factor);
	}
	//l+= vec3(0.05, 0.0, 0.0) * float(current_light_count);

	out_color= vec4(l * albedo_alpha.rgb, albedo_alpha.a);
}
