/**
 * @brief Planar shadows vertex shader.
 */

#version 120

struct FogParameters
{
	float START;
	float END;
	vec3 COLOR;
	float DENSITY;
};

uniform FogParameters FOG;

uniform mat4 MATRIX;
uniform vec4 LIGHT;

varying vec4 point;

varying float fog;

/**
 * @brief
 */
void ShadowVertex() {
	point = gl_ModelViewMatrix * MATRIX * gl_Vertex;	
}

/**
 * @brief
 */
void FogVertex(void) {
    fog = (gl_Position.z - FOG.START) / (FOG.END - FOG.START) / point.w;
    fog = clamp(fog, 0.0, 1.0) * FOG.DENSITY;
}

/**
 * @brief Program entry point.
 */
void main(void) {
	
	// mvp transform into clip space
	gl_Position = gl_ModelViewProjectionMatrix * MATRIX * gl_Vertex;
	
	// and primary color
	gl_FrontColor = gl_Color;
	
	ShadowVertex();
    
    FogVertex();
}
