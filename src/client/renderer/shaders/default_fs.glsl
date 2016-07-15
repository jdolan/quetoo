/**
 * @brief Default fragment shader.
 */

#version 120

// linear + quadratic attenuation
#define LIGHT_ATTENUATION (4.0 * dist + 8.0 * dist * dist)

// light color clamping
#define LIGHT_CLAMP_MIN 0.0
#define LIGHT_CLAMP_MAX 4.0

uniform bool DIFFUSE;
uniform bool LIGHTMAP;
uniform bool NORMALMAP;
uniform bool GLOSSMAP;

uniform float BUMP;
uniform float PARALLAX;
uniform float HARDNESS;
uniform float SPECULAR;

uniform sampler2D SAMPLER0;
uniform sampler2D SAMPLER1;
uniform sampler2D SAMPLER2;
uniform sampler2D SAMPLER3;
uniform sampler2D SAMPLER4;

varying vec3 point;
varying vec3 normal;
varying vec3 tangent;
varying vec3 bitangent;

varying float fog;

const vec3 two = vec3(2.0);
const vec3 negHalf = vec3(-0.5);

vec3 eye;

/**
 * @brief Yield the parallax offset for the texture coordinate.
 */
vec2 BumpTexcoord(in float height) {

	// transform the eye vector into tangent space for bump-mapping
	eye.x = dot(point, tangent);
	eye.y = dot(point, bitangent);
	eye.z = dot(point, normal);

	eye = -normalize(eye);

	return vec2(height * 0.04 - 0.02) * PARALLAX * eye.xy;
}

/**
 * @brief Yield the diffuse modulation from bump-mapping.
 */
vec3 BumpFragment(in vec3 deluxemap, in vec3 normalmap, in vec3 glossmap) {
	
	float diffuse = max(dot(deluxemap, normalmap), 1.0);

	float specular = HARDNESS * pow(max(-dot(eye, reflect(deluxemap, normalmap)), 0.0), 8.0 * SPECULAR);

	return diffuse + specular * glossmap;
}

/**
 * @brief Yield the final sample color after factoring in dynamic light sources. 
 */
void LightFragment(in vec4 diffuse, in vec3 lightmap, in vec3 normalmap) {

	vec3 light = vec3(0.0);

	/*
	 * Iterate the hardware light sources, accumulating dynamic lighting for
	 * this fragment.  An attenuation of 0.0 means break.
	 */
	for (int i = 0; i < gl_MaxLights; i++) {

		if (gl_LightSource[i].constantAttenuation == 0.0)
			break;

		vec3 delta = gl_LightSource[i].position.xyz - point;
		
		float dist = length(delta);
		if (dist < gl_LightSource[i].constantAttenuation) {

			float d = dot(normalmap, normalize(delta));
			if (d > 0.0) {
			
				dist = 1.0 - dist / gl_LightSource[i].constantAttenuation;
				light += gl_LightSource[i].diffuse.rgb * d * LIGHT_ATTENUATION;
			}
		}
	}

	light = clamp(light, LIGHT_CLAMP_MIN, LIGHT_CLAMP_MAX);

	// now modulate the diffuse sample with the modified lightmap
	gl_FragColor.rgb = diffuse.rgb * (lightmap + light);

	// lastly modulate the alpha channel by the color
	gl_FragColor.a = diffuse.a * gl_Color.a;
}

/**
 * @brief Apply fog to the fragment if enabled.
 */
void FogFragment(void) {
	gl_FragColor.rgb = mix(gl_FragColor.rgb, gl_Fog.color.rgb, fog);
}

/**
 * @brief Shader entry point.
 */
void main(void) {

	// first resolve the flat shading
	vec3 lightmap = gl_Color.rgb;
	vec3 deluxemap = vec3(0.0, 0.0, 1.0);

	if (LIGHTMAP) {
		lightmap = texture2D(SAMPLER1, gl_TexCoord[1].st).rgb;
	}

	// then resolve any bump mapping
	vec4 normalmap = vec4(normal, 1.0);
	vec2 parallax = vec2(0.0);
	vec3 bump = vec3(1.0);

	if (NORMALMAP) {
		deluxemap = texture2D(SAMPLER2, gl_TexCoord[1].st).rgb;
		deluxemap = normalize(two * (deluxemap + negHalf));

		normalmap = texture2D(SAMPLER3, gl_TexCoord[0].st);

		parallax = BumpTexcoord(normalmap.w);

		normalmap.xyz = normalize(two * (normalmap.xyz + negHalf));	
		normalmap.xyz = normalize(vec3(normalmap.x * BUMP, normalmap.y * BUMP, normalmap.z));	

		vec3 glossmap = vec3(1.0);

		if (GLOSSMAP) {
			glossmap = texture2D(SAMPLER4, gl_TexCoord[0].st).rgb;
		}

		// resolve the bumpmap modulation
		bump = BumpFragment(deluxemap, normalmap.xyz, glossmap);

		// and then transform the normalmap to model space for lighting
		normalmap.xyz = normalize(
			normalmap.x * normalize(tangent) +
			normalmap.y * normalize(bitangent) +
			normalmap.z * normalize(normal)
		);
	}

	vec4 diffuse = vec4(1.0);

	if (DIFFUSE) { // sample the diffuse texture, honoring the parallax offset
		diffuse = texture2D(SAMPLER0, gl_TexCoord[0].st + parallax);

		// factor in bump mapping
		diffuse.rgb *= bump;
	}

	// add any dynamic lighting to yield the final fragment color
	LightFragment(diffuse, lightmap, normalmap.xyz);

	FogFragment(); // and lastly add fog	
}
