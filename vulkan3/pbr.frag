#version 440
#define M_PI 3.141592

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
layout(location = 3) in vec3 v_Position;
layout(location = 0) out vec4 fragColor;

layout (binding = 1) uniform sampler2D shadowMap;

float clampedDot(vec3 x, vec3 y)
{
    return clamp(dot(x, y), 0.0, 1.0);
}


vec3 F_Schlick(vec3 f0, vec3 f90, float VdotH)
{
    return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

float V_GGX(float NdotL, float NdotV, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}

float D_GGX(float NdotH, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
    return alphaRoughnessSq / (M_PI * f * f);
}

vec3 BRDF_specularGGX(vec3 f0, vec3 f90, float alphaRoughness, float specularWeight, float VdotH, float NdotL, float NdotV, float NdotH)
{
    vec3 F = F_Schlick(f0, f90, VdotH);
    float Vis = V_GGX(NdotL, NdotV, alphaRoughness);
    float D = D_GGX(NdotH, alphaRoughness);

    return specularWeight * F * Vis * D;
}

vec3 BRDF_lambertian(vec3 f0, vec3 f90, vec3 diffuseColor, float specularWeight, float VdotH)
{
    // see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
    return (1.0 - specularWeight * F_Schlick(f0, f90, VdotH)) * (diffuseColor / M_PI);
}

float textureProj(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.00 && shadowCoord.z < 1.00 ) 
	{
		float dist = texture( shadowMap, shadowCoord.st + off ).r;
		if ( shadowCoord.w > 0.00 && dist < shadowCoord.z ) 
		{
			shadow = 0.0;
		}
	}
	return shadow;
}

void main()
{

    vec3 v = normalize(vec3(1, 1, 1));
    vec3 n = normalize(v_normal);


    vec3 l = v;   // Direction from surface point to light
    vec3 h = l;          // Direction of the vector between l and v, called halfway vector
    float NdotL = clampedDot(n, l);
    float NdotV = clampedDot(n, v);
    float NdotH = clampedDot(n, h);
    float LdotH = clampedDot(l, h);
    float VdotH = clampedDot(v, h);
    vec3 f_diffuse = vec3(0, 0, 0);
    vec3 f_specular = vec3(0, 0, 0);
    vec3 f0 = v_color;
    vec3 f90 = vec3(0.5);
    vec3 c_diff = v_color * (1.0 - max(max(f0.r, f0.g), f0.b));
    float specularWeight = 0.5;
    float alphaRoughness = 0.1;

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

    if (NdotL > 0.0 || NdotV > 0.0)
    {
        vec3 intensity = shadow*vec3(3.0);
        f_diffuse += intensity * NdotL *  BRDF_lambertian(f0, f90, c_diff, specularWeight, VdotH);
        f_specular += intensity * NdotL * BRDF_specularGGX(f0, f90, alphaRoughness, specularWeight, VdotH, NdotL, NdotV, NdotH);
    }



    fragColor = vec4(f_diffuse + f_specular + v_color * pc.ambience, 1.0);
}
