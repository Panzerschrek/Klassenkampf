#version 450

const float inv_letters_in_texture= 1.0 / 96.0; // TODO - make uniform

layout(location= 0) in vec2 coord;
layout(location= 1) in vec2 tex_coord;
layout(location= 2) in vec4 color;

layout(location= 0) flat out vec4 f_color;
layout(location= 1) noperspective out vec2 f_tex_coord;

void main()
{
	f_color= color;
	f_tex_coord= tex_coord * vec2( 1.0, inv_letters_in_texture );
	gl_Position= vec4( coord ,0.0, 1.0 );
}
