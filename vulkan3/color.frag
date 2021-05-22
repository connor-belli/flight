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
layout(location = 3) in vec4 location;

layout(location = 0) out vec4 fragColor;

layout (binding = 1) uniform sampler2D shadowMap;

float random (in vec2 st) {
    return fract(sin(dot(st.xy,
                         vec2(12.9898,78.233)))
                 * 43758.5453123);
}

// 2D Noise based on Morgan McGuire @morgan3d
// https://www.shadertoy.com/view/4dS3Wd
float noise (in vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);

    // Four corners in 2D of a tile
    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));

    // Smooth Interpolation

    // Cubic Hermine Curve.  Same as SmoothStep()
    vec2 u = f*f*(3.0-2.0*f);
    // u = smoothstep(0.,1.,f);

    // Mix 4 coorners percentages
    return mix(a, b, u.x) +
            (c - a)* u.y * (1.0 - u.x) +
            (d - b) * u.x * u.y;
}


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

    float noise = noise(location.xz*1);
	float strength = max(location.w/100, 4.0);
	float noiseMul = 0.5;
    float noiseFactor = noiseMul*noise/strength;

	vec3 mixedColor = mix(v_color, pc.color.xyz, pc.mixRatio);
    vec3 ambient = (ambientStrength + noiseFactor) * mixedColor;
    vec3 norm = normalize(v_normal);

    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));  
    float diff = min(max(pc.normMul*dot(norm, lightDir), 0.0), textureProj(inShadowCoord / inShadowCoord.w, vec2(0.0)));
    vec3 diffuse = diff * mixedColor;
    fragColor = vec4((diffuse*(1-pc.ambience+noiseFactor) + ambient ), 1.0);
}
