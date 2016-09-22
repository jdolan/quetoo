/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
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

#include <ctype.h>

#include "shared.h"

vec3_t vec3_origin = { 0.0, 0.0, 0.0 };

vec3_t vec3_up = { 0.0, 0.0, 1.0 };

vec3_t vec3_down = { 0.0, 0.0, -1.0 };

vec3_t vec3_forward = { 0.0, 1.0, 0.0 };

/**
 * @brief Returns a pseudo-random positive integer.
 *
 * Uses a Linear Congruence Generator, values kindly borrowed from glibc.
 * Look up the rules required for the constants before just replacing them;
 * performance is dictated by the selection.
 */
int32_t Random(void) {

	static uint32_t state = 0;
	static _Bool uninitalized = true;

	if (uninitalized) {
		state = (uint32_t) time(NULL);
		uninitalized = false;
	}

	state = (1103515245 * state + 12345);
	return state & 0x7fffffff;
}

/**
 * @brief Returns a pseudo-random vec_t between 0.0 and 1.0.
 */
vec_t Randomf(void) {
	return (Random()) * (1.0 / 0x7fffffff);
}

/**
 * @brief Returns a pseudo-random vec_t between -1.0 and 1.0.
 */
vec_t Randomc(void) {
	return (Random()) * (2.0 / 0x7fffffff) - 1.0;
}

/**
 * @brief
 */
void RotatePointAroundVector(const vec3_t point, const vec3_t dir, const vec_t degrees, vec3_t out) {
	const vec_t u = dir[0], v = dir[1], w = dir[2];
	const vec_t x = point[0], y = point[1], z = point[2];

	const vec_t ux = u * x, uy = u * y, uz = u * z;
	const vec_t vx = v * x, vy = v * y, vz = v * z;
	const vec_t wx = w * x, wy = w * y, wz = w * z;
	const vec_t uu = u * u, ww = w * w, vv = v * v;

	const vec_t s = sin(Radians(degrees));
	const vec_t c = cos(Radians(degrees));

	out[0] = u * (ux + vy + wz) + (x * (vv + ww) - u * (vy + wz)) * c + (vz - wy) * s;
	out[1] = v * (ux + vy + wz) + (y * (uu + ww) - v * (ux + wz)) * c + (wx - uz) * s;
	out[2] = w * (ux + vy + wz) + (z * (uu + vv) - w * (ux + vy)) * c + (uy - vx) * s;
}

/**
 * @brief Derives Euler angles for the specified directional vector.
 */
void VectorAngles(const vec3_t vector, vec3_t angles) {

	const vec_t forward = sqrt(vector[0] * vector[0] + vector[1] * vector[1]);

	vec_t pitch = ClampAngle(-Degrees(atan2(vector[2], forward)));
	vec_t yaw = ClampAngle(Degrees(atan2(vector[1], vector[0])));

	VectorSet(angles, pitch, yaw, 0.0);
}

/**
 * @brief Produces the forward, right and up directional vectors for the given angles.
 */
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up) {
	vec_t angle;

	angle = Radians(angles[YAW]);
	const vec_t sy = sin(angle);
	const vec_t cy = cos(angle);

	angle = Radians(angles[PITCH]);
	const vec_t sp = sin(angle);
	const vec_t cp = cos(angle);

	angle = Radians(angles[ROLL]);
	const vec_t sr = sin(angle);
	const vec_t cr = cos(angle);

	if (forward) {
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}

	if (right) {
		right[0] = -1.0 * sr * sp * cy + -1.0 * cr * -sy;
		right[1] = -1.0 * sr * sp * sy + -1.0 * cr * cy;
		right[2] = -1.0 * sr * cp;
	}

	if (up) {
		up[0] = cr * sp * cy + -sr * -sy;
		up[1] = cr * sp * sy + -sr * cy;
		up[2] = cr * cp;
	}
}

/**
 * @brief
 */
void ProjectPointOnPlane(const vec3_t p, const vec3_t normal, vec3_t out) {
	vec3_t n;

	const vec_t inv_denom = 1.0 / DotProduct(normal, normal);

	const vec_t d = DotProduct(normal, p) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	out[0] = p[0] - d * n[0];
	out[1] = p[1] - d * n[1];
	out[2] = p[2] - d * n[2];
}

/**
 * @brief Assumes input vector is normalized.
 */
void PerpendicularVector(const vec3_t in, vec3_t out) {
	int32_t pos = 0;
	vec_t min_elem = 1.0;
	vec3_t tmp;

	// find the smallest magnitude axially aligned vector
	for (int32_t i = 0; i < 3; i++) {
		if (fabsf(in[i]) < min_elem) {
			pos = i;
			min_elem = fabsf(in[i]);
		}
	}
	VectorClear(tmp);
	tmp[pos] = 1.0;

	// project the point onto the plane
	ProjectPointOnPlane(tmp, in, out);

	// normalize the result
	VectorNormalize(out);
}

/**
 * @brief Projects the normalized directional vectors on to the normal's plane.
 * The fourth component of the resulting tangent vector represents sidedness.
 */
void TangentVectors(const vec3_t normal, const vec3_t sdir, const vec3_t tdir, vec4_t tangent,
		vec3_t bitangent) {

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

/**
 * @brief Produces the linear interpolation of the two vectors for the given fraction.
 */
void VectorLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out) {

	for (int32_t i = 0; i < 3; i++)
		out[i] = from[i] + frac * (to[i] - from[i]);
}

/**
 * @brief Produces the linear interpolation of the two angles for the given fraction.
 * Care is taken to keep the values between -180.0 and 180.0.
 */
void AngleLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out) {
	vec3_t _from, _to;

	// copy the vectors to safely clamp this lerp
	VectorCopy(from, _from);
	VectorCopy(to, _to);

	for (int32_t i = 0; i < 3; i++) {

		if (_to[i] - _from[i] > 180.0)
			_to[i] -= 360.0;

		if (_to[i] - _from[i] < -180.0)
			_to[i] += 360.0;
	}

	VectorLerp(_from, _to, frac, out);
}

/**
 * @return True if the specified boxes intersect (overlap), false otherwise.
 */
_Bool BoxIntersect(const vec3_t mins0, const vec3_t maxs0, const vec3_t mins1, const vec3_t maxs1) {

	if (mins0[0] >= maxs1[0] || mins0[1] >= maxs1[1] || mins0[2] >= maxs1[2])
		return false;

	if (maxs0[0] <= mins1[0] || maxs0[1] <= mins1[1] || maxs0[2] <= mins1[2])
		return false;

	return true;
}

/**
 * @brief Initializes the specified bounds so that they may be safely calculated.
 */
void ClearBounds(vec3_t mins, vec3_t maxs) {
	mins[0] = mins[1] = mins[2] = MAX_WORLD_COORD;
	maxs[0] = maxs[1] = maxs[2] = MIN_WORLD_COORD;
}

/**
 * @brief Useful for accumulating a bounding box over a series of points.
 */
void AddPointToBounds(const vec3_t point, vec3_t mins, vec3_t maxs) {

	for (int32_t i = 0; i < 3; i++) {
		if (point[i] < mins[i])
			mins[i] = point[i];
		if (point[i] > maxs[i])
			maxs[i] = point[i];
	}
}

/**
 * @brief Normalizes the specified vector to unit-length, returning the original
 * vector's length.
 */
vec_t VectorNormalize(vec3_t v) {

	const vec_t length = VectorLength(v);
	if (length) {
		const vec_t ilength = 1.0 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

/**
 * @brief Normalizes the input vector to `out`, returning the input vector's length.
 */
vec_t VectorNormalize2(const vec3_t in, vec3_t out) {

	VectorCopy(in, out);
	return VectorNormalize(out);
}

/**
 * @brief Scales vecb and adds it to veca to produce vecc. Useful for projection.
 */
void VectorMA(const vec3_t veca, const vec_t scale, const vec3_t vecb, vec3_t vecc) {
	vecc[0] = veca[0] + scale * vecb[0];
	vecc[1] = veca[1] + scale * vecb[1];
	vecc[2] = veca[2] + scale * vecb[2];
}

/**
 * @brief Calculates the cross-product of the specified vectors.
 */
void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross) {
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

/**
 * @brief Returns the length of the specified vector.
 */
vec_t VectorLength(const vec3_t v) {
	return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/**
 * @brief Combines a fraction of the second vector with the first.
 */
void VectorMix(const vec3_t v1, const vec3_t v2, vec_t mix, vec3_t out) {

	for (int32_t i = 0; i < 3; i++)
		out[i] = v1[i] * (1.0 - mix) + v2[i] * mix;
}

/**
 * @brief Packs the specified floating point vector to the int16_t array in out
 * for network transmission.
 */
void PackVector(const vec3_t in, int16_t *out) {
	VectorScale(in, 8.0, out);
}

/**
 * @brief Unpacks the compressed vector to 32 bit floating point in out.
 */
void UnpackVector(const int16_t *in, vec3_t out) {
	VectorScale(in, 0.125, out);
}

/**
 * @brief Circular clamp Euler angles between 0.0 and 360.0.
 */
vec_t ClampAngle(vec_t angle) {

	while (angle >= 360.0) {
		angle -= 360.0;
	}
	while (angle < 0.0) {
		angle += 360.0;
	}

	return angle;
}

/**
 * @brief Circularly clamps the specified angles between 0.0 and 360.0.
 */
void ClampAngles(vec3_t angles) {
	angles[0] = ClampAngle(angles[0]);
	angles[1] = ClampAngle(angles[1]);
	angles[2] = ClampAngle(angles[2]);
}

/**
 * @brief Packs the Euler angle for network transmission, clamped 0.0 - 360.0.
 */
uint16_t PackAngle(const vec_t a) {
	return (uint16_t) (ClampAngle(a) * UINT16_MAX / 360.0) & UINT16_MAX;
}

/**
 * @brief Unpacks the encoded angle to floating point between 0.0 and 360.0.
 */
vec_t UnpackAngle(const uint16_t a) {
	return a * 360.0 / UINT16_MAX;
}

/**
 * @brief Packs the specified floating point Euler angles to the int16_t array in out
 * for network transmission.
 */
void PackAngles(const vec3_t in, uint16_t *out) {
	int32_t i;

	for (i = 0; i < 3; i++) {
		out[i] = PackAngle(in[i]);
	}
}

/**
 * @brief Unpacks the compressed angles to Euler 32 bit floating point in out.
 */
void UnpackAngles(const uint16_t *in, vec3_t out) {
	int32_t i;

	for (i = 0; i < 3; i++) {
		out[i] = UnpackAngle(in[i]);
	}
}

/**
 * @brief Packs the specified bounding box to a limited precision integer
 * representation. Bits 0-5 represent X/Y, scaled down by a factor of 0.125.
 * Bits 5-10 contain the Z-mins, and 10-15 contain the Z-maxs.
 */
void PackBounds(const vec3_t mins, const vec3_t maxs, uint16_t *out) {

	// x/y are assumed equal and symmetric
	int32_t xy = Clamp(maxs[0] * 0.125, 1.0, 31.0);

	// z is asymmetric
	int32_t zd = Clamp(-mins[2] * 0.125, 1.0, 31.0);

	// and z maxs can be negative, so shift them +32 units
	int32_t zu = Clamp((maxs[2] + 32.0) * 0.125, 1.0, 63.0);

	*out = (zu << 10) | (zd << 5) | xy;
}

/**
 * @brief Unpacks the specified bounding box to mins and maxs.
 */
void UnpackBounds(const uint16_t in, vec3_t mins, vec3_t maxs) {

	const vec_t xy = (in & 31) * 8.0;
	const vec_t zd = ((in >> 5) & 31) * 8.0;
	const vec_t zu = ((in >> 10) & 31) * 8.0 - 32.0;

	VectorSet(mins, -xy, -xy, -zd);
	VectorSet(maxs, xy, xy, zu);
}

/**
 * @brief Clamps the components of the specified vector to 1.0, scaling the vector
 * down if necessary.
 */
vec_t ColorNormalize(const vec3_t in, vec3_t out) {
	vec_t max = 0.0;

	VectorCopy(in, out);

	for (int32_t i = 0; i < 3; i++) { // find the brightest component

		if (out[i] < 0.0) // enforcing positive values
			out[i] = 0.0;

		if (out[i] > max)
			max = out[i];
	}

	if (max > 1.0) // clamp without changing hue
		VectorScale(out, 1.0 / max, out);

	return max;
}

/**
 * @brief Applies brightness, saturation and contrast to the specified input color.
 */
void ColorFilter(const vec3_t in, vec3_t out, vec_t brightness, vec_t saturation, vec_t contrast) {
	const vec3_t luminosity = { 0.2125, 0.7154, 0.0721 };

	ColorNormalize(in, out);

	if (brightness != 1.0) { // apply brightness
		VectorScale(out, brightness, out);

		ColorNormalize(out, out);
	}

	if (contrast != 1.0) { // apply contrast

		for (int32_t i = 0; i < 3; i++) {
			out[i] -= 0.5; // normalize to -0.5 through 0.5
			out[i] *= contrast; // scale
			out[i] += 0.5;
		}

		ColorNormalize(out, out);
	}

	if (saturation != 1.0) { // apply saturation
		const vec_t d = DotProduct(out, luminosity);
		vec3_t intensity;

		VectorSet(intensity, d, d, d);
		VectorMix(intensity, out, saturation, out);

		ColorNormalize(out, out);
	}
}

/**
 * @brief Returns the longest common prefix the specified words share.
 */
char *CommonPrefix(GList *words) {
	static char common_prefix[MAX_TOKEN_CHARS];

	memset(common_prefix, 0, sizeof(common_prefix));

	if (!words)
		return common_prefix;

	for (size_t i = 0; i < sizeof(common_prefix) - 1; i++) {
		GList *e = words;
		const char c = ((char *) e->data)[i];
		while (e) {
			const char *w = (char *) e->data;

			if (!c || w[i] != c) // prefix no longer common
				return common_prefix;

			e = e->next;
		}
		common_prefix[i] = c;
	}

	return common_prefix;
}

/**
 * @brief Handles wildcard suffixes for GlobMatch.
 */
static _Bool GlobMatchStar(const char *pattern, const char *text) {
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

/**
 * @brief Matches the pattern against specified text, returning true if the pattern
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
_Bool GlobMatch(const char *pattern, const char *text) {
	const char *p = pattern, *t = text;
	register char c;

	if (!p || !t) {
		return false;
	}

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
				int32_t invert;

				if (!c1)
					return 0;

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
				/* Skip the rest of the [...] construct that already matched. */
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

/**
 * @brief Returns the base name for the given file or path.
 */
const char *Basename(const char *path) {
	const char *last = path;
	while (*path) {
		if (*path == '/')
			last = path + 1;
		path++;
	}
	return last;
}

/**
 * @brief Returns the directory name for the given file or path name.
 */
void Dirname(const char *in, char *out) {
	char *c;

	if (!(c = strrchr(in, '/'))) {
		strcpy(out, "./");
		return;
	}

	while (in <= c) {
		*out++ = *in++;
	}

	*out = '\0';
}

/**
 * @brief Removes any file extension(s) from the specified input string.
 */
void StripExtension(const char *in, char *out) {

	while (*in && *in != '.') {
		*out++ = *in++;
	}

	*out = '\0';
}

/**
 * @brief Strips color escape sequences from the specified input string.
 */
void StripColors(const char *in, char *out) {

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
	*out = '\0';
}

/**
 * @brief Returns the length of s in printable characters.
 */
size_t StrColorLen(const char *s) {

	size_t len = 0;

	while (*s) {
		if (IS_COLOR(s)) {
			s += 2;
			continue;
		}

		if (IS_LEGACY_COLOR(s)) {
			s += 1;
			continue;
		}

		s++;
		len++;
	}

	return len;
}

/**
 * @brief Performs a color- and case-insensitive string comparison.
 */
int32_t StrColorCmp(const char *s1, const char *s2) {
	char string1[MAX_STRING_CHARS], string2[MAX_STRING_CHARS];

	StripColors(s1, string1);
	StripColors(s2, string2);

	return g_ascii_strcasecmp(string1, string2);
}

/**
 * @return The first color sequence in s.
 */
int32_t StrColor(const char *s) {
	
	const char *c = s;
	while (*c) {
		if (IS_COLOR(c)) {
			return *(c + 1) - '0';
		} else if (IS_LEGACY_COLOR(c)) {
			return CON_COLOR_ALT;
		}
		c++;
	}
	
	return CON_COLOR_DEFAULT;
}

/**
 * @return The last occurrence of a color escape sequence in s.
 */
int32_t StrrColor(const char *s) {
	
	if (strlen(s)) {
		const char *c = s + strlen(s) - 1;
		while (c > s) {
			if (IS_COLOR(c)) {
				return *(c + 1) - '0';
			} else if (IS_LEGACY_COLOR(c)) {
				return CON_COLOR_ALT;
			}
			c--;
		}
	}
	
	return CON_COLOR_DEFAULT;
}

/**
 * @brief A shorthand g_snprintf into a statically allocated buffer. Several
 * buffers are maintained internally so that nested va()'s are safe within
 * reasonable limits. This function is not thread safe.
 */
char *va(const char *format, ...) {
	static char strings[8][MAX_STRING_CHARS];
	static uint16_t index;

	char *string = strings[index++ % 8];

	va_list args;

	va_start(args, format);
	vsnprintf(string, MAX_STRING_CHARS, format, args);
	va_end(args);

	return string;
}

/**
 * @brief A convenience function for printing vectors.
 */
char *vtos(const vec3_t v) {
	static uint32_t index;
	static char str[8][MAX_QPATH];
	char *s = "";

	if (v) {
		s = str[index++ % 8];
		g_snprintf(s, MAX_QPATH, "(%4.2f %4.2f %4.2f)", v[0], v[1], v[2]);
	}

	return s;
}

/**
 * @brief Parse a token out of a string. Tokens are delimited by white space, and
 * may be grouped by quotation marks.
 *
 * @return The next token in the stream, or the empty string (`""`) if EOF.
 */
char *ParseToken(const char **in) {
	static char token[MAX_TOKEN_CHARS];

	if (!*in) {
		return "";
	}

	memset(&token, 0, sizeof(token));

	const char *s = *in;
	size_t len = 0;
	char c;

	// skip whitespace
	skipwhite: while ((c = *s) <= ' ') {
		if (c == '\0') {
			*in = NULL;
			return "";
		}
		s++;
	}

	// skip // comments
	if (c == '/' && s[1] == '/') {
		while (*s && *s != '\n')
			s++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"') {
		s++;
		while (true) {
			c = *s++;
			if (c == '\"' || !c) {
				token[len] = '\0';
				*in = s;
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
		s++;
		c = *s;
	} while (c > 32);

	if (len == MAX_TOKEN_CHARS) {
		len = 0;
	}
	token[len] = '\0';

	*in = s;
	return token;
}

/**
 * @brief Searches the string for the given key and returns the associated value,
 * or an empty string.
 */
char *GetUserInfo(const char *s, const char *key) {
	char pkey[512];
	static char value[2][512]; // use two buffers so compares
	// work without stomping on each other
	static int32_t value_index;
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
		*o = '\0';
		s++;

		o = value[value_index];

		while (*s != '\\' && *s) {
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = '\0';

		if (!g_strcmp0(key, pkey))
			return value[value_index];

		if (!*s)
			return "";
		s++;
	}

	return "";
}

/**
 * @brief
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
		*o = '\0';
		s++;

		o = value;
		while (*s != '\\' && *s) {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = '\0';

		if (!g_strcmp0(key, pkey)) {
			strcpy(start, s); // remove this part
			return;
		}

		if (!*s)
			return;
	}
}

/**
 * @brief Returns true if the specified user-info string appears valid, false
 * otherwise.
 */
_Bool ValidateUserInfo(const char *s) {
	if (!s || !*s)
		return false;
	if (strstr(s, "\""))
		return false;
	if (strstr(s, ";"))
		return false;
	return true;
}

/**
 * @brief
 */
void SetUserInfo(char *s, const char *key, const char *value) {
	char newi[MAX_USER_INFO_STRING], *v;

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

	if (strlen(key) > MAX_USER_INFO_KEY - 1 || strlen(value) > MAX_USER_INFO_VALUE - 1) {
		//Com_Print("Keys and values must be < 64 characters\n");
		return;
	}

	DeleteUserInfo(s, key);

	if (!value || *value == '\0')
		return;

	g_snprintf(newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) > MAX_USER_INFO_STRING) {
		//Com_Print("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newi;
	while (*v) {
		char c = *v++;
		c &= 127; // strip high bits
		if (c >= 32 && c < 127)
			*s++ = c;
	}
	*s = '\0';
}
