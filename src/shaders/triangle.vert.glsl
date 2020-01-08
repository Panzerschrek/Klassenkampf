#version 450

layout(push_constant) uniform uniforms_block
{
	vec2 pos_delta;
};

void main()
{
	vec2 pos[3]=
		vec2[3](
			vec2(-0.7, +0.7),
			vec2(+0.7, +0.7),
			vec2(+0.0, -0.7)
		);

	gl_Position= vec4(pos[gl_VertexIndex] + pos_delta, 0.0, 1.0);
}
