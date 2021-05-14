#version 440

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_color;
layout(location = 2) in vec4 inShadowCoord;

layout(location = 0) out vec4 fragColor;

layout (binding = 1) uniform sampler2D shadowMap;

float textureProj(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.00 && shadowCoord.z < 1.00 ) 
	{
		float dist = texture( shadowMap, shadowCoord.st + off ).r;
		if ( shadowCoord.w > 0.00 && dist < shadowCoord.z ) 
		{
			shadow = 0.5;
		}
	}
	return shadow;
}

void main()
{
    float ambientStrength = 0.5;
    vec3 ambient = ambientStrength * v_color;
    vec3 norm = normalize(v_normal);
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));  
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * v_color;
    fragColor = vec4((diffuse*0.5 + ambient) * textureProj(inShadowCoord / inShadowCoord.w, vec2(0.0)), 1.0);
}
