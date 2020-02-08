#version 450

layout(push_constant) uniform uniforms_block
{
	mat4 mat;
	// Use 4x4, instaead of 3x3 matrix because of problems with alignment.
	mat4 normals_mat;
};

layout(location=0) in vec3 pos;
layout(location=1) in vec2 tex_coord;
layout(location=2) in vec3 normal;

layout(location= 0) out vec3 f_normal;
layout(location= 1) out vec2 f_tex_coord;

void main()
{
	f_normal= mat3(normals_mat) * normal;
	f_tex_coord= tex_coord;
	gl_Position= mat * vec4(pos, 1.0);
}
