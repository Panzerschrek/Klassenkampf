#version 450

layout(location= 0) in vec3 f_pos; // World space position, relative to light source.

void main()
{
	const float c_abs_bias= 1.0 / 64.0; // In world units.
	const float c_bias_slope= 2.5;

	float l= (length(f_pos) + c_abs_bias) / 64.0;
	vec2 dd= vec2(dFdx(l), dFdy(l));

	gl_FragDepth= l + c_bias_slope * length(dd);
}
