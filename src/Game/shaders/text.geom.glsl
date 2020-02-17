#version 450

const float inv_letters_in_texture= 1.0 / 96.0; // TODO - make uniform

layout(points, invocations= 1) in;
layout(triangle_strip, max_vertices = 6) out;

layout(location= 0) in vec2 g_pos[];
layout(location= 1) in vec2 g_size[];
layout(location= 2) in float g_glyph_index[];
layout(location= 3) in vec4 g_color[];

layout(location= 0) out vec4 f_color;
layout(location= 1) out noperspective vec2 f_tex_coord;

void main()
{
	vec4 out_coord[4];
	vec2 out_tc[4];
	out_coord[0]= vec4(g_pos[0].x				, g_pos[0].y				, 0.0, 1.0);
	out_coord[1]= vec4(g_pos[0].x				, g_pos[0].y + g_size[0].y	, 0.0, 1.0);
	out_coord[2]= vec4(g_pos[0].x + g_size[0].x	, g_pos[0].y + g_size[0].y	, 0.0, 1.0);
	out_coord[3]= vec4(g_pos[0].x + g_size[0].x	, g_pos[0].y				, 0.0, 1.0);
	out_tc[0]= vec2(0.0, (g_glyph_index[0] + 0.0) * inv_letters_in_texture);
	out_tc[1]= vec2(0.0, (g_glyph_index[0] + 1.0) * inv_letters_in_texture);
	out_tc[2]= vec2(1.0, (g_glyph_index[0] + 1.0) * inv_letters_in_texture);
	out_tc[3]= vec2(1.0, (g_glyph_index[0] + 0.0) * inv_letters_in_texture);

	f_color= g_color[0];

	gl_Position= out_coord[0];
	f_tex_coord= out_tc[0];
	EmitVertex();
	gl_Position= out_coord[1];
	f_tex_coord= out_tc[1];
	EmitVertex();
	gl_Position= out_coord[2];
	f_tex_coord= out_tc[2];
	EmitVertex();
	EndPrimitive();

	f_color= g_color[0];

	gl_Position= out_coord[0];
	f_tex_coord= out_tc[0];
	EmitVertex();
	gl_Position= out_coord[2];
	f_tex_coord= out_tc[2];
	EmitVertex();
	gl_Position= out_coord[3];
	f_tex_coord= out_tc[3];
	EmitVertex();
	EndPrimitive();
}
