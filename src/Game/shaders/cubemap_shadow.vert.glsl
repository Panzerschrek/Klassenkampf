#version 450

// in/out - world space position.
layout(location= 0) in vec3 pos;
layout(location= 0) out vec3 g_pos;

void main()
{
	g_pos= pos;
}
