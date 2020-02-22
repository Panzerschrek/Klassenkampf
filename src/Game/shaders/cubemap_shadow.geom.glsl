#version 450

layout(triangles, invocations= 6) in;
layout(triangle_strip, max_vertices = 3) out;

layout(binding= 0, std430) buffer readonly matrices_block
{
	mat4 view_matrices[6];
	vec3 light_pos;
};

layout(location= 0) in vec3 g_pos[];

layout(location= 0) out vec3 f_pos;

void main()
{
	int i= gl_InvocationID;
	gl_Layer= i;

	gl_Position= view_matrices[i] * vec4(g_pos[0], 1.0);
	f_pos= g_pos[0] - light_pos;
	EmitVertex();

	gl_Position= view_matrices[i] * vec4(g_pos[1], 1.0);
	f_pos= g_pos[1] - light_pos;
	EmitVertex();

	gl_Position= view_matrices[i] * vec4(g_pos[2], 1.0);
	f_pos= g_pos[2] - light_pos;
	EmitVertex();

	EndPrimitive();
}
