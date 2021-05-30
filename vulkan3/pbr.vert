#version 440

layout(location = 0) in vec4 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_color;
layout(location = 2) out vec4 outShadowCoord;
layout(location = 3) out vec3 v_Position;
layout(std140, binding = 0) uniform buf {
    mat4 mvp;
	mat4 cameraSpace;
	mat4 model;
} ubuf;

out gl_PerVertex { vec4 gl_Position; };

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void main()
{
    v_normal = (ubuf.model*vec4(normal, 0.0f)).xyz;
    v_color = color;
	vec4 pos = ubuf.model * position;
	v_Position = pos.xyz / pos.w;
    gl_Position = ubuf.mvp * position;
	outShadowCoord = ( biasMat * ubuf.cameraSpace * ubuf.model) * position;
}
