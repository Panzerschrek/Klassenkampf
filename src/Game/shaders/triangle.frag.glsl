#version 450

layout(binding= 0) uniform sampler2D tex;

layout(binding= 1) buffer readonly light_buffer_block
{
	// Use vec4 for fit alignment.
	vec4 light_pos;
	vec4 light_color;
	vec4 ambient_light_color;
};

layout(location= 0) in vec3 f_normal;
layout(location= 1) in vec2 f_tex_coord;

layout(location = 0) out vec4 out_color;

void main()
{
	const vec3 sun_vector= normalize(vec3(0.5, 0.2, 0.7));
	vec3 l= max(0.0, dot(light_pos.xyz, normalize(f_normal))) * light_color.xyz + ambient_light_color.xyz;

	const vec4 tex_value= texture(tex, f_tex_coord);
	if(tex_value.a < 0.5)
		discard;

	out_color= vec4(l * tex_value.xyz, tex_value.a);
}
