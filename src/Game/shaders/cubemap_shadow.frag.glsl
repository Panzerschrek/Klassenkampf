#version 450

layout(location= 0) in vec3 f_pos; // World space position, relative to light source.

void main()
{
	gl_FragDepth= length(f_pos) / 64.0f;
}
