/*
 * @brief Warp fragment shader.
 */

#version 120

uniform vec3 OFFSET;

uniform sampler2D SAMPLER0;
uniform sampler2D SAMPLER1;

varying float fog;

/*
 * FogFragment
 */
void FogFragment(void) {
	gl_FragColor.rgb = mix(gl_FragColor.rgb, gl_Fog.color.rgb, fog);
}

/*
 * main
 */
void main(void) {

	// sample the warp texture at a time-varied offset
	vec4 warp = texture2D(SAMPLER1, gl_TexCoord[0].xy + OFFSET.xy);

	// and derive a diffuse texcoord based on the warp data
	vec2 coord = vec2(gl_TexCoord[0].x + warp.z, gl_TexCoord[0].y + warp.w);

	// sample the diffuse texture, factoring in primary color as well
	gl_FragColor = gl_Color * texture2D(SAMPLER0, coord);

	FogFragment();  // add fog
}
