
/**
 * @brief BSP vertex shader.
 */

#version 330

uniform mat4 projection;
uniform mat4 model_view;
uniform mat4 normal;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_tangent;
layout (location = 3) in vec3 in_bitangent;
layout (location = 4) in vec2 in_diffuse;
layout (location = 5) in vec2 in_lightmap;
layout (location = 6) in vec4 in_color;

out vertex_data {
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffuse;
	vec2 lightmap;
	vec4 color;
	vec3 eye;
} vertex;

/**
 * @brief
 */
void main(void) {

	gl_Position = projection * model_view * vec4(in_position, 1.0);

	vertex.position = (model_view * vec4(in_position, 1.0)).xyz;
	vertex.normal = normalize(vec3(normal * vec4(in_normal, 1.0)));
	vertex.tangent = normalize(vec3(normal * vec4(in_tangent, 1.0)));
	vertex.bitangent = normalize(vec3(normal * vec4(in_bitangent, 1.0)));

	vertex.diffuse = in_diffuse;
	vertex.lightmap = in_lightmap;
	vertex.color = in_color;

	vertex.eye.x = -dot(vertex.position, vertex.tangent);
	vertex.eye.y = -dot(vertex.position, vertex.bitangent);
	vertex.eye.z = -dot(vertex.position, vertex.normal);
}
