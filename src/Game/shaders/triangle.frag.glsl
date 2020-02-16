#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require


struct Light
{
	vec4 pos;
	vec4 color;
};

layout(binding= 0) uniform sampler2D tex;

layout(binding= 1, std430) buffer readonly light_buffer_block
{
	// Use vec4 for fit alignment.
	vec4 ambient_color;
	ivec4 light_count;
	ivec4 cluster_volume_size;
	vec4 viewport_size;
	Light lights[];
};

layout(binding= 2, std430) buffer readonly cluster_offset_buffer_block
{
	int light_offsets[];
};

layout(binding= 3, std430) buffer readonly lights_list_buffer_block
{
	uint8_t light_list[];
};

layout(location= 0) in vec3 f_normal;
layout(location= 1) in vec2 f_tex_coord;
layout(location= 2) in vec3 f_pos; // World space position.

layout(location = 0) out vec4 out_color;

void main()
{
	vec3 normal_normalized= normalize(f_normal);
	vec2 frag_coord_normalized= gl_FragCoord.xy / viewport_size.xy;

	vec3 cluster_coord= vec3(cluster_volume_size.xyz) * vec3(frag_coord_normalized, pow(gl_FragCoord.z, 64.0));
	int offset= int(light_offsets[
		int(cluster_coord.x) +
		int(cluster_coord.y) * cluster_volume_size.x +
		int(cluster_coord.z) * (cluster_volume_size.x * cluster_volume_size.y) ]);

	vec3 l= ambient_color.rgb;
	int current_light_count= light_list[offset];
	for(int i= 0; i < current_light_count; ++i)
	{
		Light light= lights[ int(light_list[offset + 1 + i]) ];
		vec3 vec_to_light= light.pos.xyz - f_pos;
		//float k= max(dot(f_normal, vec_to_light), 0.0) / (dot(vec_to_light, vec_to_light) * length(vec_to_light));

		float k= 1.0 - smoothstep(0.0, light.pos.w, length(vec_to_light));
		k*= max(dot(normal_normalized, normalize(vec_to_light)), 0.0);
		l+= light.color.xyz * k;
	}
	//l+= vec3(0.05, 0.0, 0.0) * float(current_light_count);

	const vec4 tex_value= texture(tex, f_tex_coord);
	if(tex_value.a < 0.5)
		discard;

	out_color= vec4(l * tex_value.xyz, tex_value.a);
}
