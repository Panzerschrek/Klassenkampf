#version 450

layout(binding= 0) uniform sampler2D tex;

layout(push_constant) uniform uniforms_block
{
	vec3 sun_vector;
};

layout(location= 0) in vec3 f_normal;
layout(location= 1) in vec2 f_tex_coord;

layout(location = 0) out vec4 out_color;

void main()
{
	float l= max(0.0, dot(sun_vector, normalize(f_normal))) + 0.25;

	const vec4 tex_value= texture(tex, f_tex_coord);
	if(tex_value.a < 0.5)
		discard;

	out_color= vec4(l * tex_value.xyz, tex_value.a);
}
