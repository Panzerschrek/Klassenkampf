#version 450

const float c_smooth_edge= 0.15;

layout(binding= 0) uniform sampler2D tex;

layout(location= 0) flat in vec4 f_color;
layout(location= 1) noperspective in vec2 f_tex_coord;

layout(location= 0) out vec4 out_color;

void main()
{
	vec2 tex_coord_pixels= f_tex_coord * vec2(textureSize(tex, 0));
	vec2 ddx= dFdx(tex_coord_pixels);
	vec2 ddy= dFdy(tex_coord_pixels);
	float texels_per_pixel= sqrt(max(dot(ddx, ddx), dot(ddy, ddy)));

	// x - SDF, y - simple smooth font
	vec2 tex_value= texture(tex, f_tex_coord).xy;

	float smooth_range= min(texels_per_pixel * c_smooth_edge, 0.5);

	float alpha = smoothstep(max(0, 0.15 - smooth_range), min(1.0, 0.15 + smooth_range), tex_value.x);
	float border= smoothstep(max(0, 0.5  - smooth_range), min(1.0, 0.5  + smooth_range), tex_value.x);

	// if we draw downscale text, blend SDF-based font with simple font
	float font_tye_blend_k= smoothstep(1.0, 2.0, texels_per_pixel);
	alpha= mix(alpha, tex_value.y, font_tye_blend_k);
	border= (1.0 - border) * (1.0 - font_tye_blend_k);

	vec3 color= mix(f_color.xyz, vec3(1.0, 1.0, 1.0) - f_color.xyz, border);
	out_color= vec4(color, alpha * f_color.a);
}
