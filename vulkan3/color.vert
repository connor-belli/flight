#version 440

layout(location = 0) in vec4 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_color;
layout(location = 2) out vec4 outShadowCoord;
layout(location = 3) out vec4 location;

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
    gl_Position = ubuf.mvp * position;
	outShadowCoord = ( biasMat * ubuf.cameraSpace * ubuf.model) * position;
	float x = position.x * length(vec3(ubuf.model[0][0], ubuf.model[0][1], ubuf.model[0][2]));
	float y = position.y * length(vec3(ubuf.model[1][0], ubuf.model[1][1], ubuf.model[1][2]));
	float z = position.z * length(vec3(ubuf.model[2][0], ubuf.model[2][1], ubuf.model[2][2]));
	location = vec4(x, y, z, gl_Position.w);
}
