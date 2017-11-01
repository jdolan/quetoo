/**
 * @brief Particle geometry shader.
 */

#version 330

#define GEOMETRY_SHADER

#include "include/uniforms.glsl"

// the number of vertices for both ends of a wire.
#define CYLINDER_VERT_COUNT 16
#define CYLINDER_SIDES (CYLINDER_VERT_COUNT / 2)
#define PI 3.1415926535897932384626433832795
#define CYLINDER_STEP (1.0 / (CYLINDER_SIDES - 1)) * 2.0

layout (points) in;
layout (triangle_strip, max_vertices = CYLINDER_VERT_COUNT) out;

uniform vec3 VIEW_ORIGIN;
uniform vec3 VIEW_ANGLES;
uniform vec3 VIEW_RIGHT;
uniform vec3 VIEW_UP;
uniform vec3 WEATHER_RIGHT;
uniform vec3 WEATHER_UP;
uniform vec3 SPLASH_RIGHT[2];
uniform vec3 SPLASH_UP[2];

in VertexData {
	vec2 texcoord0;
	vec2 texcoord1;
	vec4 color;
	float scale;
	float roll;
	vec3 end;
	int type;
} in_data[];

out VertexData {
	vec4 color;
	vec2 texcoord;
	vec3 point;
} out_data;

void CopyCommon(void) {
	out_data.point = (MODELVIEW_MAT * gl_in[0].gl_Position).xyz;
	out_data.color = in_data[0].color;
}

#define PARTICLE_SPARK 1
#define PARTICLE_ROLL 2
#define PARTICLE_EXPLOSION 3
#define PARTICLE_BEAM 5
#define PARTICLE_WEATHER 6
#define PARTICLE_SPLASH 7
#define PARTICLE_WIRE 10

#define Radians(d) 					((d) * 0.01745329251) // * M_PI / 180.0
#define Degrees(r)					((r) * 57.2957795131) // * 180.0 / M_PI

#define atan2 atan

#include "include/uniforms.glsl"

/**
 * @brief Circular clamp Euler angles between 0.0 and 360.0.
 */
#define ClampAngle(a) mod(a, 360.0)

/**
 * @brief Derives Euler angles for the specified directional vector.
 */
void VectorAngles(in vec3 vector, out vec3 angles) {
	float forward = sqrt(vector[0] * vector[0] + vector[1] * vector[1]);

	float pitch = ClampAngle(-Degrees(atan2(vector[2], forward)));
	float yaw = ClampAngle(Degrees(atan2(vector[1], vector[0])));

	angles.x = pitch;
	angles.y = yaw;
	angles.z = 0.0;
}

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

	if (in_data[0].type == PARTICLE_WIRE) { // wire particle is turned into a cylinder
		vec3 fwd, right, up;
		int side = 0;
		float st_stepsize = (st2.y - st1.y) / CYLINDER_SIDES;
		vec3 angles;

		VectorAngles(normalize(org - in_data[0].end), angles);

		AngleVectors(angles, fwd, right, up);

		for (int i = 0; i < CYLINDER_SIDES; i++) {
			float step = (i * CYLINDER_STEP) * PI;
			float st_step = st_stepsize * i;
			float s = sin(step);
			float c = cos(step);
			vec3 dir = ((right * -c) + (up * s)) * in_data[0].scale;
			float st_l = st0.y + st_step;
			float st_r = st0.y + (st_step + st_stepsize);

			// end point
			gl_Position = MVP * vec4(in_data[0].end + dir, 1.0);
			out_data.texcoord = st3;
			out_data.texcoord.y = st_l;
			CopyCommon();
			EmitVertex();

			side = (side + 1) % 4;

			// start point
			gl_Position = MVP * vec4(org + dir, 1.0);
			out_data.texcoord = st0;
			out_data.texcoord.y = st_l;
			CopyCommon();
			EmitVertex();

			side = (side + 1) % 4;
		}

		EndPrimitive();
		return;
	} else if (in_data[0].type == PARTICLE_BEAM || in_data[0].type == PARTICLE_SPARK) { // beams are lines with starts and ends
		vec3 up = normalize(org - in_data[0].end);
		vec3 right = normalize(cross(up, VIEW_ORIGIN - in_data[0].end)) * in_data[0].scale;

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
		vec3 dir = vec3(VIEW_ANGLES.xy, in_data[0].roll * (TIME / 1000.0));
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
