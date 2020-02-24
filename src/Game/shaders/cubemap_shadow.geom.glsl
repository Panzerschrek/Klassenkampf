#version 450

layout(triangles, invocations= 6) in;
layout(triangle_strip, max_vertices = 3) out;

layout(binding= 0, std430) buffer readonly matrices_block
{
	mat4 view_matrices[6];
};

layout(push_constant) uniform uniforms_block
{
	vec4 light_pos;
};

layout(location= 0) in vec3 g_pos[];

layout(location= 0) out vec3 f_pos;

void main()
{
	int i= gl_InvocationID;
	gl_Layer= i;

	for( int j= 0; j < 3; ++j)
	{
		vec3 pos_relative= g_pos[j] - light_pos.xyz;
		gl_Position= view_matrices[i] * vec4(pos_relative, 1.0);
		f_pos= pos_relative * light_pos.w;
		EmitVertex();
	}

	EndPrimitive();
}
