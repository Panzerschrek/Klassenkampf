#version 450

layout(push_constant) uniform uniforms_block
{
	vec4 blur_vector;
};

layout(binding= 0) uniform sampler2D tex;

layout(location= 0) in noperspective vec2 f_tex_coord;

layout(location= 0) out vec4 color;

void main()
{
	vec4 r= vec4(0.0, 0.0, 0.0, 0.0);
	for(int i= -5; i <= +5; ++i)
		r+= texture(tex, f_tex_coord + float(i) * blur_vector.xy);
	color= r * (1.0 / 11.0);
}
