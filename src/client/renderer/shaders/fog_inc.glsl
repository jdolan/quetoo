#ifndef FOG_NO_UNIFORM
struct FogParameters
{
	float START;
	float END;
	vec3 COLOR;
	float DENSITY;
};

uniform FogParameters FOG;
#endif

varying float fog;
