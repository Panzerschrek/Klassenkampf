#version 450

layout(location= 0) in vec3 f_pos; // World space position, relative to light source.

void main()
{
	const float c_constant_bias= 2.0 / 65536.0; // 2 values of 16-bit depth
	const float c_bias_slope= 2.5;

	float l= length(f_pos);
	vec2 dd= vec2(dFdx(l), dFdy(l));

	gl_FragDepth= l + c_constant_bias + c_bias_slope * length(dd);
}
