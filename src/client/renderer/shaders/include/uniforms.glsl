#ifndef QUETOO_UNIFORMS_GLSL
#define QUETOO_UNIFORMS_GLSL

/**
 * @brief Uniform interface block. Shared across programs.
 * THIS DATA NEEDS TO BE LAID OUT IN A VERY SPECIFIC MANNER. Please don't
 * touch this without also looking at 
 */

layout (std140) uniform SharedData {
	mat4 PROJECTION_MAT;
	mat4 MODELVIEW_MAT;
	mat4 SHADOW_MAT;
	vec4 GLOBAL_COLOR;
	float TIME;
	float TIME_FRACTION;
};

#endif //QUETOO_UNIFORMS_GLSL
