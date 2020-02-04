#version 450

layout(binding= 1) uniform uniform_block
{
	mat4 other_mat;
};

layout(location=0) in vec3 pos;
layout(location=1) in vec2 tex_coord;
layout(location=2) in vec3 normal;

layout(location= 0) out vec3 f_normal;
layout(location= 1) out vec2 f_tex_coord;

void main()
{
	f_normal= normal;
	f_tex_coord= tex_coord;
	gl_Position= other_mat * vec4(pos, 1.0);
}
