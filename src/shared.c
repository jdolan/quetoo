/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "shared.h"
#include "files.h"

vec3_t vec3_origin = { 0.0, 0.0, 0.0 };

vec3_t PM_MINS = { -16.0, -16.0, -24.0 };
vec3_t PM_MAXS = { 16.0, 16.0, 40.0 };

/*
 * frand
 *
 * Returns a float between 0.0 and 1.0.
 */
float frand(void) {
	return (rand() & 32767) * (1.0 / 32767);
}

/*
 * crand
 *
 * Returns a float between -1.0 and 1.0.
 */
float crand(void) {
	return (rand() & 32767) * (2.0 / 32767) - 1.0;
}

/*
 * RotatePointAroundVector
 */
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point,
		float degrees) {
	float m[3][3];
	float im[3][3];
	float zrot[3][3];
	float tmpmat[3][3];
	float rot[3][3];
	int i;
	vec3_t vr, vu, vf;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector(vr, dir);
	CrossProduct(vr, vf, vu);

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vu[0];
	m[1][1] = vu[1];
	m[2][1] = vu[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	memcpy(im, m, sizeof(im));

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	memset(zrot, 0, sizeof(zrot));
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	zrot[0][0] = cos(DEG2RAD(degrees));
	zrot[0][1] = sin(DEG2RAD(degrees));
	zrot[1][0] = -sin(DEG2RAD(degrees));
	zrot[1][1] = cos(DEG2RAD(degrees));

	ConcatRotations(m, zrot, tmpmat);
	ConcatRotations(tmpmat, im, rot);

	for (i = 0; i < 3; i++) {
		dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2]
				* point[2];
	}
}

/*
 * VectorAngles
 *
 * Derives Euler angles for the specified directional vector.
 */
void VectorAngles(const vec3_t vector, vec3_t angles) {
	const float forward = sqrt(vector[0] * vector[0] + vector[1] * vector[1]);
	float pitch = atan2(vector[2], forward) * 180.0 / M_PI;
	const float yaw = atan2(vector[1], vector[0]) * 180.0 / M_PI;

	if (pitch < 0.0) {
		pitch += 360.0;
	}

	VectorSet(angles, -pitch, yaw, 0.0);
}

/*
 * AngleVectors
 *
 * Produces the forward, right and up directional vectors for the given angles.
 */
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up) {
	float angle;
	float sr, sp, sy, cr, cp, cy;

	angle = angles[YAW] * (M_PI * 2.0 / 360.0);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI * 2.0 / 360.0);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI * 2.0 / 360.0);
	sr = sin(angle);
	cr = cos(angle);

	if (forward) {
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}
	if (right) {
		right[0] = -1 * sr * sp * cy + -1 * cr * -sy;
		right[1] = -1 * sr * sp * sy + -1 * cr * cy;
		right[2] = -1 * sr * cp;
	}
	if (up) {
		up[0] = cr * sp * cy + -sr * -sy;
		up[1] = cr * sp * sy + -sr * cy;
		up[2] = cr * cp;
	}
}

/*
 * ProjectPointOnPlane
 */
void ProjectPointOnPlane(vec3_t dst, const vec3_t p, const vec3_t normal) {
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = 1.0F / DotProduct(normal, normal);

	d = DotProduct(normal, p) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

/*
 * PerpendicularVector
 *
 * Assumes src vector is normalized.
 */
void PerpendicularVector(vec3_t dst, const vec3_t src) {
	int pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec;

	// find the smallest magnitude axially aligned vector
	for (pos = 0, i = 0; i < 3; i++) {
		if (fabsf(src[i]) < minelem) {
			pos = i;
			minelem = fabsf(src[i]);
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	// project the point onto the plane defined by src
	ProjectPointOnPlane(dst, tempvec, src);

	// normalize the result
	VectorNormalize(dst);
}

/*
 * TangentVectors
 *
 * Projects the normalized directional vectors on to the normal's plane.
 * The fourth component of the resulting tangent vector represents sidedness.
 */
void TangentVectors(const vec3_t normal, const vec3_t sdir, const vec3_t tdir,
		vec4_t tangent, vec3_t bitangent) {

	vec3_t s, t;

	// normalize the directional vectors
	VectorCopy(sdir, s);
	VectorNormalize(s);

	VectorCopy(tdir, t);
	VectorNormalize(t);

	// project the directional vector onto the plane
	VectorMA(s, -DotProduct(s, normal), normal, tangent);
	VectorNormalize(tangent);

	// resolve sidedness, encode as fourth tangent component
	CrossProduct(normal, tangent, bitangent);

	if (DotProduct(t, bitangent) < 0.0)
		tangent[3] = -1.0;
	else
		tangent[3] = 1.0;

	VectorScale(bitangent, tangent[3], bitangent);
}

/*
 * ConcatRotations
 */
void ConcatRotations(vec3_t in1[3], vec3_t in2[3], vec3_t out[3]) {
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2]
			* in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2]
			* in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2]
			* in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2]
			* in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2]
			* in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2]
			* in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2]
			* in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2]
			* in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2]
			* in2[2][2];
}

/*
 * VectorLerp
 *
 * Produces the linear interpolation of the two vectors for the given fraction.
 */
void VectorLerp(const vec3_t from, const vec3_t to, const vec_t frac,
		vec3_t out) {
	int i;

	for (i = 0; i < 3; i++)
		out[i] = from[i] + frac * (to[i] - from[i]);
}

/*
 * AngleLerp
 *
 * Produces the linear interpolation of the two angles for the given fraction.
 * Care is taken to keep the values between -180.0 and 180.0.
 */
void AngleLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out) {
	vec3_t _from, _to;
	int i;

	// copy the vectors to safely clamp this lerp
	VectorCopy(from, _from);
	VectorCopy(to, _to);

	for (i = 0; i < 3; i++) {

		if (_to[i] - _from[i] > 180)
			_to[i] -= 360;

		if (_to[i] - _from[i] < -180)
			_to[i] += 360;
	}

	VectorLerp(_from, _to, frac, out);
}

/*
 * R_SignBitsForPlane
 *
 * Returns a bit mask hinting at the sign of each normal vector component. This
 * is used as an optimization for the box-on-plane-side test.
 */
byte SignBitsForPlane(const c_bsp_plane_t *plane) {
	byte i, bits = 0;

	for (i = 0; i < 3; i++) {
		if (plane->normal[i] < 0)
			bits |= 1 << i;
	}

	return bits;
}

/*
 * BoxOnPlaneSide
 *
 * Returns the sidedness of the given bounding box relative to the specified
 * plane. If the box straddles the plane, this function returns PSIDE_BOTH.
 */
int BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs,
		const struct c_bsp_plane_s *p) {
	float dist1, dist2;
	int sides;

	// axial planes
	if (AXIAL(p)) {
		if (p->dist - SIDE_EPSILON <= emins[p->type])
			return SIDE_FRONT;
		if (p->dist + SIDE_EPSILON >= emaxs[p->type])
			return SIDE_BACK;
		return SIDE_BOTH;
	}

	// general case
	switch (p->sign_bits) {
	case 0:
		dist1 = DotProduct(p->normal, emaxs);
		dist2 = DotProduct(p->normal, emins);
		break;
	case 1:
		dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1]
				+ p->normal[2] * emaxs[2];
		dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1]
				+ p->normal[2] * emins[2];
		break;
	case 2:
		dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1]
				+ p->normal[2] * emaxs[2];
		dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1]
				+ p->normal[2] * emins[2];
		break;
	case 3:
		dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1]
				+ p->normal[2] * emaxs[2];
		dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1]
				+ p->normal[2] * emins[2];
		break;
	case 4:
		dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1]
				+ p->normal[2] * emins[2];
		dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1]
				+ p->normal[2] * emaxs[2];
		break;
	case 5:
		dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1]
				+ p->normal[2] * emins[2];
		dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1]
				+ p->normal[2] * emaxs[2];
		break;
	case 6:
		dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1]
				+ p->normal[2] * emins[2];
		dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1]
				+ p->normal[2] * emaxs[2];
		break;
	case 7:
		dist1 = DotProduct(p->normal, emins);
		dist2 = DotProduct(p->normal, emaxs);
		break;
	default:
		dist1 = dist2 = 0.0; // shut up compiler
		break;
	}

	sides = 0;
	if (dist1 >= p->dist)
		sides = SIDE_FRONT;
	if (dist2 < p->dist)
		sides |= SIDE_BACK;

	return sides;
}

/*
 * ClearBounds
 *
 * Initializes the specified bounds so that they may be safely calculated.
 */
void ClearBounds(vec3_t mins, vec3_t maxs) {
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}

/*
 * AddPointToBounds
 *
 * Useful for accumulating a bounding box over a series of points.
 */
void AddPointToBounds(const vec3_t point, vec3_t mins, vec3_t maxs) {
	int i;

	for (i = 0; i < 3; i++) {
		if (point[i] < mins[i])
			mins[i] = point[i];
		if (point[i] > maxs[i])
			maxs[i] = point[i];
	}
}

/*
 * VectorCompare
 *
 * Returns true if the specified vectors are equal, false otherwise.
 */
boolean_t VectorCompare(const vec3_t v1, const vec3_t v2) {

	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2])
		return false;

	return true;
}

/*
 * VectorNearer
 *
 * Returns true if the first vector is closer to the point of interest, false
 * otherwise.
 */
boolean_t VectorNearer(const vec3_t v1, const vec3_t v2, const vec3_t point) {
	vec3_t d1, d2;

	VectorSubtract(point, v1, d1);
	VectorSubtract(point, v2, d2);

	return VectorLength(d1) < VectorLength(d2);
}

/*
 * VectorNormalize
 *
 * Normalizes the specified vector to unit-length, returning the original
 * vector's length.
 */
vec_t VectorNormalize(vec3_t v) {
	float length, ilength;

	length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

	if (length) {
		ilength = 1.0 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

/*
 * VectorMA
 *
 * Scales vecb and adds it to veca to produce vecc.  Useful for projection.
 */
void VectorMA(const vec3_t veca, const float scale, const vec3_t vecb,
		vec3_t vecc) {
	vecc[0] = veca[0] + scale * vecb[0];
	vecc[1] = veca[1] + scale * vecb[1];
	vecc[2] = veca[2] + scale * vecb[2];
}

/*
 * CrossProduct
 *
 * Calculates the cross-product of the specified vectors.
 */
void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross) {
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

/*
 * VectorLength
 *
 * Returns the length of the specified vector.
 */
vec_t VectorLength(const vec3_t v) {
	return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/*
 * VectorMix
 *
 * Combines a fraction of the second vector with the first.
 */
void VectorMix(const vec3_t v1, const vec3_t v2, float mix, vec3_t out) {
	int i;

	for (i = 0; i < 3; i++)
		out[i] = v1[i] * (1.0 - mix) + v2[i] * mix;
}

/*
 * ColorNormalize
 *
 * Clamps the components of the specified vector to 1.0, scaling the vector
 * down if necessary.
 */
vec_t ColorNormalize(const vec3_t in, vec3_t out) {
	vec_t max = 0.0;
	int i;

	VectorCopy(in, out);

	for (i = 0; i < 3; i++) { // find the brightest component

		if (out[i] < 0.0) // enforcing positive values
			out[i] = 0.0;

		if (out[i] > max)
			max = out[i];
	}

	if (max > 1.0) // clamp without changing hue
		VectorScale(out, 1.0 / max, out);

	return max;
}

/*
 * ColorFilter
 *
 * Applies brightness, saturation and contrast to the specified input color.
 */
void ColorFilter(const vec3_t in, vec3_t out, float brightness,
		float saturation, float contrast) {
	const vec3_t luminosity = { 0.2125, 0.7154, 0.0721 };
	vec3_t intensity;
	float d;
	int i;

	// apply global scale factor
	VectorScale(in, brightness, out);

	ColorNormalize(out, out);

	for (i = 0; i < 3; i++) { // apply contrast

		out[i] -= 0.5; // normalize to -0.5 through 0.5

		out[i] *= contrast; // scale

		out[i] += 0.5;
	}

	ColorNormalize(out, out);

	// apply saturation
	d = DotProduct(out, luminosity);

	VectorSet(intensity, d, d, d);
	VectorMix(intensity, out, saturation, out);

	ColorNormalize(out, out);
}

/*
 * MixedCase
 *
 * Returns true if the specified string has some upper case characters.
 */
boolean_t MixedCase(const char *s) {
	const char *c = s;
	while (*c) {
		if (isupper(*c))
			return true;
		c++;
	}
	return false;
}

/*
 * Lowercase
 *
 * Lowercases the specified string.
 */
char *Lowercase(char *s) {
	char *c = s;
	while (*c) {
		*c = tolower(*c);
		c++;
	}
	return s;
}

/*
 * Trim
 *
 * Trims leading and trailing whitespace from the specified string.
 */
char *Trim(char *s) {
	char *left, *right;

	left = s;

	while (isspace(*left))
		left++;

	right = left + strlen(left) - 1;

	while (isspace(*right))
		*right-- = 0;

	return left;
}

/*
 * CommonPrefix
 *
 * Returns the longest common prefix the specified words share.
 */
char *CommonPrefix(const char *words[], unsigned int nwords) {
	static char common_prefix[MAX_TOKEN_CHARS];
	const char *w;
	char c;
	unsigned int i, j;

	memset(common_prefix, 0, sizeof(common_prefix));

	if (!words || !words[0])
		return common_prefix;

	for (i = 0; i < sizeof(common_prefix); i++) {
		j = c = 0;
		while (j < nwords) {

			w = words[j];

			if (!w[i]) // we've exhausted our shortest match
				return common_prefix;

			if (!c) // first word this iteration
				c = w[i];

			if (w[i] != c) // prefix no longer common
				return common_prefix;

			j++;
		}
		common_prefix[i] = c;
	}

	return common_prefix;
}

/*
 * GlobMatchStar
 *
 * Handles wildcard suffixes for GlobMatch.
 */
static boolean_t GlobMatchStar(const char *pattern, const char *text) {
	const char *p = pattern, *t = text;
	register char c, c1;

	while ((c = *p++) == '?' || c == '*')
		if (c == '?' && *t++ == '\0')
			return false;

	if (c == '\0')
		return true;

	if (c == '\\')
		c1 = *p;
	else
		c1 = c;

	while (true) {
		if ((c == '[' || *t == c1) && GlobMatch(p - 1, t))
			return true;
		if (*t++ == '\0')
			return false;
	}

	return false;
}

/*
 * GlobMatch
 *
 * Matches the pattern against specified text, returning true if the pattern
 * matches, false otherwise.
 *
 * A match means the entire string TEXT is used up in matching.
 *
 * In the pattern string, `*' matches any sequence of characters,
 * `?' matches any character, [SET] matches any character in the specified set,
 * [!SET] matches any character not in the specified set.
 *
 * A set is composed of characters or ranges; a range looks like
 * character hyphen character(as in 0-9 or A-Z).
 * [0-9a-zA-Z_] is the set of characters allowed in C identifiers.
 * Any other character in the pattern must be matched exactly.
 *
 * To suppress the special syntactic significance of any of `[]*?!-\',
 * and match the character exactly, precede it with a `\'.
 */
boolean_t GlobMatch(const char *pattern, const char *text) {
	const char *p = pattern, *t = text;
	register char c;

	while ((c = *p++) != '\0')
		switch (c) {
		case '?':
			if (*t == '\0')
				return 0;
			else
				++t;
			break;

		case '\\':
			if (*p++ != *t++)
				return 0;
			break;

		case '*':
			return GlobMatchStar(p, t);

		case '[': {
			register char c1 = *t++;
			int invert;

			if (!c1)
				return (0);

			invert = ((*p == '!') || (*p == '^'));
			if (invert)
				p++;

			c = *p++;
			while (true) {
				register char cstart = c, cend = c;

				if (c == '\\') {
					cstart = *p++;
					cend = cstart;
				}
				if (c == '\0')
					return 0;

				c = *p++;
				if (c == '-' && *p != ']') {
					cend = *p++;
					if (cend == '\\')
						cend = *p++;
					if (cend == '\0')
						return 0;
					c = *p++;
				}
				if (c1 >= cstart && c1 <= cend)
					goto match;
				if (c == ']')
					break;
			}
			if (!invert)
				return 0;
			break;

			match:
			/* Skip the rest of the [...] construct that already matched.  */
			while (c != ']') {
				if (c == '\0')
					return 0;
				c = *p++;
				if (c == '\0')
					return 0;
				else if (c == '\\')
					++p;
			}
			if (invert)
				return 0;
			break;
		}

		default:
			if (c != *t++)
				return 0;
			break;
		}

	return *t == '\0';
}

/*
 * Basename
 *
 * Returns the base name for the given file or path.
 */
const char *Basename(const char *path) {
	const char *last;

	last = path;
	while (*path) {
		if (*path == '/')
			last = path + 1;
		path++;
	}
	return last;
}

/*
 * Dirname
 *
 * Returns the directory name for the given file or path name.
 */
void Dirname(const char *in, char *out) {
	char *c;

	if (!(c = strrchr(in, '/'))) {
		strcpy(out, "./");
		return;
	}

	strncpy(out, in, (c - in) + 1);
	out[(c - in) + 1] = 0;
}

/*
 * StripExtension
 *
 * Removes any file extension(s) from the specified input string.
 */
void StripExtension(const char *in, char *out) {
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

/*
 * StripColor
 *
 * Strips color escape sequences from the specified input string.
 */
void StripColor(const char *in, char *out) {

	while (*in) {

		if (IS_COLOR(in)) {
			in += 2;
			continue;
		}

		if (IS_LEGACY_COLOR(in)) {
			in++;
			continue;
		}

		*out++ = *in++;
	}
	*out = 0;
}

/*
 * StrColorCmp
 *
 * Performs a color- and case-insensitive string comparison.
 */
int StrColorCmp(const char *s1, const char *s2) {
	char string1[MAX_STRING_CHARS], string2[MAX_STRING_CHARS];

	StripColor(s1, string1);
	StripColor(s2, string2);

	return strcasecmp(string1, string2);
}

/*
 * va
 *
 * A shorthand sprintf into a temp buffer.
 */
char *va(const char *format, ...) {
	va_list args;
	static char string[MAX_STRING_CHARS];

	memset(string, 0, sizeof(string));

	va_start(args, format);
	vsnprintf(string, sizeof(string), format, args);
	va_end(args);

	return string;
}

/*
 * ParseToken
 *
 * Parse a token out of a string. Tokens are delimited by white space, and
 * may be grouped by quotation marks.
 */
char *ParseToken(const char **data_p) {
	static char token[MAX_TOKEN_CHARS];
	int c;
	int len;
	const char *data;

	data = *data_p;
	len = 0;
	token[0] = 0;

	if (!data) {
		*data_p = NULL;
		return "";
	}

	// skip whitespace
	skipwhite: while ((c = *data) <= ' ') {
		if (c == 0) {
			*data_p = NULL;
			return "";
		}
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"') {
		data++;
		while (true) {
			c = *data++;
			if (c == '\"' || !c) {
				token[len] = 0;
				*data_p = data;
				return token;
			}
			if (len < MAX_TOKEN_CHARS) {
				token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do {
		if (len < MAX_TOKEN_CHARS) {
			token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c > 32);

	if (len == MAX_TOKEN_CHARS) {
		len = 0;
	}
	token[len] = 0;

	*data_p = data;
	return token;
}

/*
 * GetUserInfo
 *
 * Searches the string for the given key and returns the associated value,
 * or an empty string.
 */
char *GetUserInfo(const char *s, const char *key) {
	char pkey[512];
	static char value[2][512]; // use two buffers so compares
	// work without stomping on each other
	static int value_index;
	char *o;

	value_index ^= 1;
	if (*s == '\\')
		s++;
	while (true) {
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[value_index];

		while (*s != '\\' && *s) {
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp(key, pkey))
			return value[value_index];

		if (!*s)
			return "";
		s++;
	}

	return "";
}

/*
 * DeleteUserInfo
 */
void DeleteUserInfo(char *s, const char *key) {
	char *start;
	char pkey[512];
	char value[512];
	char *o;

	if (strstr(key, "\\")) {
		return;
	}

	while (true) {
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s) {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp(key, pkey)) {
			strcpy(start, s); // remove this part
			return;
		}

		if (!*s)
			return;
	}
}

/*
 * ValidateUserInfo
 *
 * Returns true if the specified user-info string appears valid, false
 * otherwise.
 */
boolean_t ValidateUserInfo(const char *s) {
	if (strstr(s, "\""))
		return false;
	if (strstr(s, ";"))
		return false;
	return true;
}

/*
 * SetUserInfo
 */
void SetUserInfo(char *s, const char *key, const char *value) {
	char newi[MAX_USER_INFO_STRING], *v;
	int c;
	unsigned int max_size = MAX_USER_INFO_STRING;

	if (strstr(key, "\\") || strstr(value, "\\")) {
		//Com_Print("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr(key, ";")) {
		//Com_Print("Can't use keys or values with a semicolon\n");
		return;
	}

	if (strstr(key, "\"") || strstr(value, "\"")) {
		//Com_Print("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) > MAX_USER_INFO_KEY - 1 || strlen(value)
			> MAX_USER_INFO_VALUE - 1) {
		//Com_Print("Keys and values must be < 64 characters.\n");
		return;
	}
	DeleteUserInfo(s, key);
	if (!value || *value == '\0')
		return;

	snprintf(newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) > max_size) {
		//Com_Print("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newi;
	while (*v) {
		c = *v++;
		c &= 127; // strip high bits
		if (c >= 32 && c < 127)
			*s++ = c;
	}
	*s = 0;
}

/*
 * ColorByName
 */
int ColorByName(const char *s, int def) {
	int i;

	if (!s || *s == '\0')
		return def;

	i = atoi(s);
	if (i > 0 && i < 255)
		return i;

	if (!strcasecmp(s, "red"))
		return 242;
	if (!strcasecmp(s, "green"))
		return 209;
	if (!strcasecmp(s, "blue"))
		return 243;
	if (!strcasecmp(s, "yellow"))
		return 219;
	if (!strcasecmp(s, "orange"))
		return 225;
	if (!strcasecmp(s, "white"))
		return 216;
	if (!strcasecmp(s, "pink"))
		return 247;
	if (!strcasecmp(s, "purple"))
		return 187;
	return def;
}
