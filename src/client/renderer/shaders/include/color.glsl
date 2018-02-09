#define BRIGHTNESS_SCALE $r_brightness
#define SATURATION_SCALE $r_saturation
#define CONTRAST_SCALE $r_contrast
#define MONOCHROME $r_monochrome
#define INVERT $r_invert

vec4 ColorFilter(in vec4 color)
{
#if MONOCHROME
	color.rgb = vec3((color.r + color.g + color.b) / 3.0);
#endif

#if INVERT
	color.rgb = vec3(1.0) - color.rgb;
#endif

	color.rgb = mix(vec3(0.5), mix(vec3(dot(vec3(0.2125, 0.7154, 0.0721), color.rgb * BRIGHTNESS_SCALE)), color.rgb * BRIGHTNESS_SCALE, SATURATION_SCALE), CONTRAST_SCALE);

	return color;
}