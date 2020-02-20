#version 450

layout(push_constant) uniform uniforms_block
{
	mat4 mat;
};

layout(location=0) in vec3 pos;
layout(location=1) in vec2 tex_coord;
layout(location=2) in vec3 normal;

layout(location= 0) out vec3 f_normal;
layout(location= 1) out vec2 f_tex_coord;
layout(location= 2) out vec3 f_pos;

void main()
{
	f_normal= normal;
	f_tex_coord= tex_coord;
	f_pos= pos;
	gl_Position= mat * vec4(pos, 1.0);
}
