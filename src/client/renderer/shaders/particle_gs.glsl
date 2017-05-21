#version 330

#define FOG_NO_UNIFORM
#include "fog_inc.glsl"
#include "matrix_inc.glsl"

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

uniform vec3 VIEW_ORIGIN;
uniform vec3 VIEW_ANGLES;
uniform vec3 VIEW_RIGHT;
uniform vec3 VIEW_UP;
uniform vec3 WEATHER_RIGHT;
uniform vec3 WEATHER_UP;
uniform vec3 SPLASH_RIGHT[2];
uniform vec3 SPLASH_UP[2];
uniform float TICKS;

in VertexData {
	vec4 color;
	vec2 texcoord0;
	vec2 texcoord1;
	float scale;
	float roll;
	vec3 end;
	int type;
	FOG_VARIABLE;
} in_data[];

out VertexData {
	vec4 color;
	vec2 texcoord;
	FOG_VARIABLE;
} out_data;

void CopyCommon(void) {
	out_data.color = in_data[0].color;
	out_data.fog = in_data[0].fog;
}

#define PARTICLE_SPARK 1
#define PARTICLE_ROLL 2
#define PARTICLE_EXPLOSION 3
#define PARTICLE_BEAM 5
#define PARTICLE_WEATHER 6
#define PARTICLE_SPLASH 7

#define Radians(d) 					((d) * 0.01745329251) // * M_PI / 180.0
#define Degrees(r)					((r) * 57.2957795131) // * 180.0 / M_PI

/**
 * @brief Produces the forward, right and up directional vectors for the given angles.
 */
void AngleVectors(in vec3 angles, out vec3 forward, out vec3 right, out vec3 up) {
	float angle;

	angle = Radians(angles[1]);
	float sy = sin(angle);
	float cy = cos(angle);

	angle = Radians(angles[0]);
	float sp = sin(angle);
	float cp = cos(angle);

	angle = Radians(angles[2]);
	float sr = sin(angle);
	float cr = cos(angle);

	forward[0] = cp * cy;
	forward[1] = cp * sy;
	forward[2] = -sp;

	right[0] = -1.0 * sr * sp * cy + -1.0 * cr * -sy;
	right[1] = -1.0 * sr * sp * sy + -1.0 * cr * cy;
	right[2] = -1.0 * sr * cp;

	up[0] = cr * sp * cy + -sr * -sy;
	up[1] = cr * sp * sy + -sr * cy;
	up[2] = cr * cp;
}

void main(void) {
	mat4 MVP = PROJECTION_MAT * MODELVIEW_MAT;
	vec3 org = gl_in[0].gl_Position.xyz;
	vec2 st0 = vec2(in_data[0].texcoord0[0], in_data[0].texcoord0[1]);
	vec2 st1 = vec2(in_data[0].texcoord1[0], in_data[0].texcoord0[1]);
	vec2 st3 = vec2(in_data[0].texcoord1[0], in_data[0].texcoord1[1]);
	vec2 st2 = vec2(in_data[0].texcoord0[0], in_data[0].texcoord1[1]);

	if (in_data[0].type == PARTICLE_BEAM || in_data[0].type == PARTICLE_SPARK) { // beams are lines with starts and ends
		vec3 v = org - in_data[0].end;
		v = normalize(v);
		vec3 up = v;

		v = VIEW_ORIGIN - in_data[0].end;
		vec3 right = cross(up, v);
		right = normalize(right);
		right = right * in_data[0].scale;

		// vertex 0
		gl_Position = MVP * vec4(org + right, 1.0);
		out_data.texcoord = st0;
		CopyCommon();
		EmitVertex();

		// vertex 1
		gl_Position = MVP * vec4(in_data[0].end + right, 1.0);
		out_data.texcoord = st1;
		CopyCommon();
		EmitVertex();

		// vertex 3
		gl_Position = MVP * vec4(org - right, 1.0);
		out_data.texcoord = st2;
		CopyCommon();
		EmitVertex();

		// vertex 2
		gl_Position = MVP * vec4(in_data[0].end - right, 1.0);
		out_data.texcoord = st3;
		CopyCommon();
		EmitVertex();

		EndPrimitive();
		return;
	}

	vec3 right, up;

	if (in_data[0].type == PARTICLE_WEATHER) { // keep it vertical

		right = WEATHER_RIGHT * in_data[0].scale;
		up = WEATHER_UP * in_data[0].scale;
	} else if (in_data[0].type == PARTICLE_SPLASH) { // keep it horizontal

		if (org.z > VIEW_ORIGIN.z) { // it's above us
			right = SPLASH_RIGHT[0] * in_data[0].scale;
			up = SPLASH_UP[0] * in_data[0].scale;
		} else { // it's below us
			right = SPLASH_RIGHT[1] * in_data[0].scale;
			up = SPLASH_UP[1] * in_data[0].scale;
		}
	} else if (in_data[0].type == PARTICLE_ROLL || in_data[0].type == PARTICLE_EXPLOSION) { // roll it
		vec3 dir = VIEW_ANGLES;
		dir.z = in_data[0].roll * TICKS;

		vec3 fwd;

		AngleVectors(dir, fwd, right, up);

		right *= in_data[0].scale;
		up *= in_data[0].scale;
	} else { // default particle alignment with view

		right = VIEW_RIGHT * in_data[0].scale;
		up = VIEW_UP * in_data[0].scale;
	}

	vec3 up_right = up + right;
	vec3 down_right = right - up;

	// vertex 0
	gl_Position = MVP * vec4(org - down_right, 1.0);
	out_data.texcoord = st0;
	CopyCommon();
	EmitVertex();

	// vertex 1
	gl_Position = MVP * vec4(org + up_right, 1.0);
	out_data.texcoord = st1;
	CopyCommon();
	EmitVertex();

	// vertex 3
	gl_Position = MVP * vec4(org - up_right, 1.0);
	out_data.texcoord = st2;
	CopyCommon();
	EmitVertex();

	// vertex 2
	gl_Position = MVP * vec4(org + down_right, 1.0);
	out_data.texcoord = st3;
	CopyCommon();
	EmitVertex();

	EndPrimitive();
}