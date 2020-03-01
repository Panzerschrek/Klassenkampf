#version 450

layout(push_constant) uniform uniforms_block
{
	mat4 mat;
};

layout(location=0) in vec3 pos;
layout(location=1) in vec2 tex_coord;
layout(location=2) in vec3 normal;
layout(location=3) in vec3 binormal;
layout(location=4) in vec3 tangent;

layout(location= 0) out mat3 f_texture_space_mat; // mat3 is 3 vectors
layout(location= 3) out vec2 f_tex_coord;
layout(location= 4) out vec3 f_pos;

void main()
{
	f_texture_space_mat= mat3(binormal, tangent, normal);
	f_tex_coord= tex_coord;
	f_pos= pos;
	gl_Position= mat * vec4(pos, 1.0);
}
