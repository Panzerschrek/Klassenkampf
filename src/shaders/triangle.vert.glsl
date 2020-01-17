#version 450

layout(push_constant) uniform uniforms_block
{
	mat4 mat;
};

layout(location=0) in vec3 pos;
layout(location=1) in vec4 color;

layout(location= 0) out vec4 f_color;
layout(location= 1) out vec2 f_tex_coord;

void main()
{
	f_color= color;
	f_tex_coord= pos.xy + vec2(0.5, 0.5);
	gl_Position= mat * vec4(pos, 1.0);
}
