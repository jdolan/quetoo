/**
 * @brief BSP fragment shader.
 */

#version 330

#define MAX_ACTIVE_LIGHTS 10

#define BSP_TEXTURE_DIFFUSE     (1 << 0)
#define BSP_TEXTURE_NORMALMAP   (1 << 1)
#define BSP_TEXTURE_GLOSSMAP    (1 << 2)

#define BSP_TEXTURE_LIGHTMAP    (1 << 3)
#define BSP_TEXTURE_DELUXEMAP   (1 << 4)
#define BSP_TEXTURE_STAINMAP    (1 << 5)

uniform int textures;

uniform sampler2D texture_diffuse;
uniform sampler2D texture_normalmap;
uniform sampler2D texture_glossmap;
uniform sampler2DArray texture_lightmap;

uniform int contents;

uniform vec4 color;
uniform float alpha_threshold;

uniform float brightness;
uniform float contrast;
uniform float saturation;
uniform float modulate;

uniform float bump;
uniform float parallax;
uniform float hardness;
uniform float specular;

uniform vec4 light_positions[MAX_ACTIVE_LIGHTS];
uniform vec3 light_colors[MAX_ACTIVE_LIGHTS];

uniform vec3 fog_parameters;
uniform vec3 fog_color;

uniform vec4 caustics;

in vertex_data {
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffuse;
	vec2 lightmap;
	vec4 color;
	vec3 eye;
} vertex;

out vec4 out_color;

/**
 * @brief
 */
vec4 ColorFilter(in vec4 color) {

	vec3 bias = vec3(0.5);
	vec3 luminance = vec3(0.2125, 0.7154, 0.0721);

	color.rgb = mix(bias, mix(vec3(dot(luminance, color.rgb * brightness)), color.rgb * brightness, saturation), contrast);

	return color;
}

/**
 * @brief
 */
void main(void) {

	vec4 diffuse;
	if ((textures & BSP_TEXTURE_DIFFUSE) == BSP_TEXTURE_DIFFUSE) {
		diffuse = texture(texture_diffuse, vertex.diffuse);

		if (diffuse.a < alpha_threshold) {
			discard;
		}
	} else {
		diffuse = color;
	}

	vec4 normalmap;
	if ((textures & BSP_TEXTURE_NORMALMAP) == BSP_TEXTURE_NORMALMAP) {
		normalmap = texture(texture_normalmap, vertex.diffuse);
		normalmap.xyz = normalize(normalmap.xyz);
		normalmap.xy = (normalmap.xy * 2.0 - 1.0) * bump;
		normalmap.xyz = normalize(normalmap.xyz);
	} else {
		normalmap = vec4(0.0, 0.0, 1.0, 0.5);
	}

	vec4 glossmap;
	if ((textures & BSP_TEXTURE_GLOSSMAP) == BSP_TEXTURE_GLOSSMAP) {
		glossmap = texture(texture_glossmap, vertex.diffuse);
	} else {
		glossmap = vec4(1.0);
	}

	vec3 lightmap;
	if ((textures & BSP_TEXTURE_LIGHTMAP) == BSP_TEXTURE_LIGHTMAP) {
		lightmap = texture(texture_lightmap, vec3(vertex.lightmap, 0)).rgb * modulate;
	} else {
		lightmap = vec3(1.0);
	}

	vec3 deluxemap;
	if ((textures & BSP_TEXTURE_DELUXEMAP) == BSP_TEXTURE_DELUXEMAP) {
		deluxemap = normalize(texture(texture_lightmap, vec3(vertex.lightmap, 1)).xyz);
	} else {
		deluxemap = vec3(0.0, 0.0, 1.0);
	}

	vec4 stainmap;
	if ((textures & BSP_TEXTURE_STAINMAP) == BSP_TEXTURE_STAINMAP) {
		stainmap = texture(texture_lightmap, vec3(vertex.lightmap, 2));
	} else {
		stainmap = vec4(0.0);
	}

	out_color = diffuse * vec4(lightmap, 1.0) /** dot(normalmap.xyz, deluxemap.xyz)*/;

	out_color = ColorFilter(out_color);
}
