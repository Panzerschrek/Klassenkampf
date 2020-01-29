#version 450

layout(binding= 0) uniform sampler2D tex;

layout(location= 0) in vec3 f_normal;
layout(location= 1) in vec2 f_tex_coord;

layout(location = 0) out vec4 out_color;

void main()
{
	const vec3 sun_vector= normalize(vec3(0.5, 0.2, 0.7));
	float l= max(0.0, dot(sun_vector, normalize(f_normal))) + 0.25;

	out_color= l * texture(tex, f_tex_coord);
}
