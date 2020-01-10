#version 450

layout(push_constant) uniform uniforms_block
{
	vec2 pos_delta;
};

layout(location=0) in vec3 pos;

void main()
{
	gl_Position= vec4(pos.xy + pos_delta, pos.z, 1.0);
}
