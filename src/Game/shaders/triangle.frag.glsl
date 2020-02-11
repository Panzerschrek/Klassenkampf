#version 450

layout(binding= 0) uniform sampler2D tex;
layout(binding= 2) uniform sampler2DShadow shadowmap;

layout(binding= 1) buffer readonly light_buffer_block
{
	// Use vec4 for fit alignment.
	vec4 light_pos;
	vec4 light_color;
	vec4 ambient_light_color;
	mat4 shadowmap_mat;
};

layout(location= 0) in vec3 f_normal;
layout(location= 1) in vec2 f_tex_coord;
layout(location= 2) in vec3 f_pos;

layout(location = 0) out vec4 out_color;

void main()
{
	vec4 shadow_pos= shadowmap_mat * vec4(f_pos, 1.0);
	shadow_pos.z-= 0.01;
	float shadowmap_value= texture(shadowmap, shadow_pos.xyz * vec3(0.5, 0.5, 1.0) + vec3(0.5, 0.5, 0.0));
	float direct_light= shadowmap_value * max(0.0, dot(light_pos.xyz, normalize(f_normal)));
	vec3 l= direct_light * light_color.xyz + ambient_light_color.xyz;

	const vec4 tex_value= texture(tex, f_tex_coord);
	if(tex_value.a < 0.5)
		discard;

	out_color= vec4(l * tex_value.xyz, tex_value.a);
}
