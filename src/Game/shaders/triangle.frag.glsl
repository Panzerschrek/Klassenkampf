#version 450

layout(binding= 0) uniform sampler2D tex;

layout(binding= 1, std430) buffer readonly light_buffer_block
{
	// Use vec4 for fit alignment.
	vec4 ambient_color;
};

layout(location= 0) in vec3 f_normal;
layout(location= 1) in vec2 f_tex_coord;

layout(location = 0) out vec4 out_color;

void main()
{
	vec3 l= ambient_color.rgb;

	const vec4 tex_value= texture(tex, f_tex_coord);
	if(tex_value.a < 0.5)
		discard;

	out_color= vec4(l * tex_value.xyz, tex_value.a);
}
