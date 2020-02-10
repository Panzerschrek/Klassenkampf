#version 450

layout(binding= 0) uniform sampler2D tex;

layout(location= 0) in vec2 f_tex_coord; // In range [-1; 1]

layout(location = 0) out vec4 out_color;

vec3 tonemapping_function(vec3 x, float exposure)
{
	// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
	x*= exposure;
	float a= 2.51;
	float b= 0.03;
	float c= 2.43;
	float d= 0.59;
	float e= 0.14;
	return (x * (a * x + b))/(x * (c * x + d) + e);
}

void main()
{
	const vec3 k= vec3(9.0, 8.5, 8.0);
	float scale_base= dot(f_tex_coord, f_tex_coord);
	vec2 tex_coord_r= f_tex_coord * ((scale_base + k.r) * (0.5 / (2.0 + k.r))) + vec2(0.5, 0.5);
	vec2 tex_coord_g= f_tex_coord * ((scale_base + k.g) * (0.5 / (2.0 + k.g))) + vec2(0.5, 0.5);
	vec2 tex_coord_b= f_tex_coord * ((scale_base + k.b) * (0.5 / (2.0 + k.b))) + vec2(0.5, 0.5);

	vec3 color=
		vec3(
			texture(tex, tex_coord_r).r,
			texture(tex, tex_coord_g).g,
			texture(tex, tex_coord_b).b
			);

	float exposure= 1.0; // TODO - read uniform
	color= tonemapping_function(color, exposure);

	out_color= vec4(color, 1.0);
}
