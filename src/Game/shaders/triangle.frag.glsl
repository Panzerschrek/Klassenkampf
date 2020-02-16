#version 450

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
	Light lights[];
};

layout(location= 0) in vec3 f_normal;
layout(location= 1) in vec2 f_tex_coord;
layout(location= 2) in vec3 f_pos; // World space position.

layout(location = 0) out vec4 out_color;

void main()
{
	vec3 normal_normalized= normalize(f_normal);

	vec3 l= ambient_color.rgb;
	for(int i= 0; i < light_count.x; ++i)
	{
		vec3 vec_to_light= lights[i].pos.xyz - f_pos;
		float k= max(dot(f_normal, vec_to_light), 0.0) / (dot(vec_to_light, vec_to_light) * length(vec_to_light));
		l+= lights[i].color.xyz * k;
	}

	const vec4 tex_value= texture(tex, f_tex_coord);
	if(tex_value.a < 0.5)
		discard;

	out_color= vec4(l * tex_value.xyz, tex_value.a);
}
