/**
 * @brief Planar shadows vertex shader.
 */

#version 120

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
    fog = (gl_Position.z - gl_Fog.start) / (gl_Fog.end - gl_Fog.start) / point.w;
    fog = clamp(fog, 0.0, 1.0) * gl_Fog.density;
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
