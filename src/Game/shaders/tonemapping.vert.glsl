#version 450

layout(binding= 1) uniform sampler2D brightness_tex;

layout(location= 0) out noperspective vec2 f_tex_coord;
layout(location= 1) out flat float f_exposure;

void main()
{
	vec2 pos[6]=
			vec2[6](
				vec2(-1.0, -1.0),
				vec2(+1.0, -1.0),
				vec2(+1.0, +1.0),

				vec2(-1.0, -1.0),
				vec2(+1.0, +1.0),
				vec2(-1.0, +1.0)
			);

	gl_Position= vec4(pos[gl_VertexIndex], 0.0, 1.0);
	f_tex_coord= pos[gl_VertexIndex];

	float brightness= dot(textureLod(brightness_tex, vec2(0.5, 0.5), 16).rgb, vec3(0.299, 0.587, 0.114));
	f_exposure= 0.6 * pow(brightness + 0.001, -0.75);
}
