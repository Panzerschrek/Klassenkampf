#version 450


layout(push_constant) uniform uniforms_block
{
	vec2 pos_scale;
	vec2 size_scale;
};

layout(location= 0) in vec2 pos;
layout(location= 1) in float size;
layout(location= 2) in vec4 color;
layout(location= 3) in float glyph_index;

layout(location= 0) out vec2 g_pos;
layout(location= 1) out vec2 g_size;
layout(location= 2) out float g_glyph_index;
layout(location= 3) out vec4 g_color;

void main()
{
	g_pos= pos * pos_scale;
	g_size= size * size_scale;
	g_glyph_index= glyph_index;
	g_color= color;
}
