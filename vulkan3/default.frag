#version 440

layout( push_constant ) uniform constants
{
	vec4 color;
	float ambience;
	float normMul;
	float mixRatio;
} pc;

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
			shadow = (1-pc.ambience);
		}
	}
	return shadow;
}

void main()
{
    float ambientStrength = pc.ambience;

	vec3 mixedColor = mix(v_color, pc.color.xyz, pc.mixRatio);
    vec3 ambient = (ambientStrength) * mixedColor;
    vec3 norm = normalize(v_normal);
	vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
	float shadow = 0.0;
	for(int x = -1; x <= 1; ++x)
	{
		for(int y = -1; y <= 1; ++y)
		{
			shadow += textureProj(inShadowCoord / inShadowCoord.w, vec2(x, y)*texelSize);
		}    
	}
	shadow /= 9.0;

    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));  
    float diff = min(max(pc.normMul*dot(norm, lightDir), 0.0), shadow);
    vec3 diffuse = diff * mixedColor;
    fragColor = vec4((diffuse*(1-pc.ambience) + ambient ), 1.0);
}
