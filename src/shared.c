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

vec3_t vec3_origin = {0.0, 0.0, 0.0};

vec3_t PM_MINS = { -16.0, -16.0, -24.0};
vec3_t PM_MAXS = {  16.0,  16.0,  40.0};


/*
 * frand
 *
 * Returns a float between 0.0 and 1.0.
 */
float frand(void){
	return(rand() & 32767) * (1.0 / 32767);
}


/*
 * crand
 *
 * Returns a float between -1.0 and 1.0.
 */
float crand(void){
	return(rand() & 32767) * (2.0 / 32767) - 1.0;
}


/*
 * RotatePointAroundVector
 */
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees){
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

	for(i = 0; i < 3; i++){
		dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
	}
}


/*
 * VectorAngles
 */
void VectorAngles(const vec3_t vector, vec3_t angles){
	const float forward = sqrt(vector[0] * vector[0] + vector[1] * vector[1]);
	float pitch = atan2(vector[2], forward) * 180.0 / M_PI;
	const float yaw = atan2(vector[1], vector[0]) * 180.0 / M_PI;

	if(pitch < 0.0)
		pitch += 360.0;

	angles[PITCH] = -pitch;
	angles[YAW] =  yaw;
	angles[ROLL] = 0.0;
}


/*
 * AngleVectors
 */
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up){
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

	if(forward){
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}
	if(right){
		right[0] = -1 * sr * sp * cy + -1 * cr * -sy;
		right[1] = -1 * sr * sp * sy + -1 * cr * cy;
		right[2] = -1 * sr * cp;
	}
	if(up){
		up[0] = cr * sp * cy + -sr * -sy;
		up[1] = cr * sp * sy + -sr * cy;
		up[2] = cr * cp;
	}
}


/*
 * ProjectPointOnPlane
 */
void ProjectPointOnPlane(vec3_t dst, const vec3_t p, const vec3_t normal){
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
void PerpendicularVector(vec3_t dst, const vec3_t src){
	int pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec;

	// find the smallest magnitude axially aligned vector
	for(pos = 0, i = 0; i < 3; i++){
		if(fabsf(src[i]) < minelem){
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
		vec4_t tangent, vec3_t bitangent){

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

	if(DotProduct(t, bitangent) < 0.0)
		tangent[3] = -1.0;
	else
		tangent[3] = 1.0;

	VectorScale(bitangent, tangent[3], bitangent);
}


/*
 * ConcatRotations
 */
void ConcatRotations(vec3_t in1[3], vec3_t in2[3], vec3_t out[3]){
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
}


/*
 * VectorLerp
 */
void VectorLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out){
	int i;

	for(i = 0; i < 3; i++)
		out[i] = from[i] + frac * (to[i] - from[i]);
}


/*
 * AngleLerp
 */
void AngleLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out){
	vec3_t _from, _to;
	int i;

	// copy the vectors to safely clamp this lerp
	VectorCopy(from, _from);
	VectorCopy(to, _to);

	for(i = 0; i < 3; i++){

		if(_to[i] - _from[i] > 180)
			_to[i] -= 360;

		if(_to[i] - _from[i] < -180)
			_to[i] += 360;
	}

	VectorLerp(_from, _to, frac, out);
}


/*
 * BoxOnPlaneSide
 *
 * Returns 1, 2, or 1 + 2
 */
int BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const struct cplane_s *p){
	float dist1, dist2;
	int sides;

	// axial planes
	if(AXIAL(p)){
		if(p->dist - PLANESIDE_EPSILON <= emins[p->type])
			return PSIDE_FRONT;
		if(p->dist + PLANESIDE_EPSILON >= emaxs[p->type])
			return PSIDE_BACK;
		return PSIDE_BOTH;
	}

	// general case
	switch(p->signbits){
		case 0:
			dist1 = DotProduct(p->normal, emaxs);
			dist2 = DotProduct(p->normal, emins);
			break;
		case 1:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			break;
		case 2:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			break;
		case 3:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			break;
		case 4:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			break;
		case 5:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			break;
		case 6:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			break;
		case 7:
			dist1 = DotProduct(p->normal, emins);
			dist2 = DotProduct(p->normal, emaxs);
			break;
		default:
			dist1 = dist2 = 0;  // shut up compiler
			break;
	}

	sides = 0;
	if(dist1 >= p->dist)
		sides = PSIDE_FRONT;
	if(dist2 < p->dist)
		sides |= PSIDE_BACK;

	return sides;
}


/*
 * ClearBounds
 */
void ClearBounds(vec3_t mins, vec3_t maxs){
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}


/*
 * AddPointToBounds
 */
void AddPointToBounds(const vec3_t point, vec3_t mins, vec3_t maxs){
	int i;

	for(i = 0; i < 3; i++){
		if(point[i] < mins[i])
			mins[i] = point[i];
		if(point[i] > maxs[i])
			maxs[i] = point[i];
	}
}


/*
 * VectorCompare
 */
qboolean VectorCompare(const vec3_t v1, const vec3_t v2){

	if(v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2])
		return false;

	return true;
}


/*
 * VectorNearer
 */
qboolean VectorNearer(const vec3_t v1, const vec3_t v2, const vec3_t comp){
	vec3_t d1, d2;

	VectorSubtract(comp, v1, d1);
	VectorSubtract(comp, v2, d2);

	return VectorLength(d1) < VectorLength(d2);
}


/*
 * VectorNormalize
 */
vec_t VectorNormalize(vec3_t v){
	float length, ilength;

	length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

	if(length){
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
void VectorMA(const vec3_t veca, const float scale, const vec3_t vecb, vec3_t vecc){
	vecc[0] = veca[0] + scale * vecb[0];
	vecc[1] = veca[1] + scale * vecb[1];
	vecc[2] = veca[2] + scale * vecb[2];
}


/*
 * CrossProduct
 */
void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross){
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}


/*
 * VectorLength
 */
vec_t VectorLength(const vec3_t v){
	return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}


/*
 * VectorMix
 */
void VectorMix(const vec3_t v1, const vec3_t v2, float mix, vec3_t out){
	int i;

	for(i = 0; i < 3; i++)
		out[i] = v1[i] * (1.0 - mix) + v2[i] * mix;
}


/*
 * ColorNormalize
 */
vec_t ColorNormalize(const vec3_t in, vec3_t out){
	vec_t max;


	// find the brightest component
	max = in[0];

	if(in[1] > max)
		max = in[1];

	if(in[2] > max)
		max = in[2];

	if(max == 0.0){  // avoid FPE
		VectorClear(out);
		return 0.0;
	}

	VectorScale(in, 1.0 / max, out);

	return max;
}


/*
 * Com_Mixedcase
 */
qboolean Com_Mixedcase(const char *s){
	const char *c = s;
	while(*c){
		if(isupper(*c))
			return true;
		c++;
	}
	return false;
}


/*
 * Com_Lowercase
 */
char *Com_Lowercase(char *s){
	char *c = s;
	while(*c){
		*c = tolower(*c);
		c++;
	}
	return s;
}


/*
 * Com_Trim
 */
char *Com_Trim(char *s){
	char *left, *right;

	left = s;

	while(isspace(*left))
		left++;

	right = left + strlen(left) - 1;

	while(isspace(*right))
		*right-- = 0;

	return left;
}


static char common_prefix[MAX_TOKEN_CHARS];

/*
 * Com_CommonPrefix
 */
char *Com_CommonPrefix(const char *words[], int nwords){
	const char *w;
	char c;
	int i, j;

	memset(common_prefix, 0, sizeof(common_prefix));

	if(!words || !words[0])
		return common_prefix;

	for(i = 0; i < sizeof(common_prefix); i++){
		j = c = 0;
		while(j < nwords){

			w = words[j];

			if(!w[i])  // we've exhausted our shortest match
				return common_prefix;

			if(!c)  // first word this iteration
				c = w[i];

			if(w[i] != c)  // prefix no longer common
				return common_prefix;

			j++;
		}
		common_prefix[i] = c;
	}

	return common_prefix;
}


/*
 * Like glob_match, but match pattern against any final segment of text.
 */
int Com_GlobMatchStar(const char *pattern, const char *text){
	const char *p = pattern, *t = text;
	register char c, c1;

	while((c = *p++) == '?' || c == '*')
		if(c == '?' && *t++ == '\0')
			return 0;

	if(c == '\0')
		return 1;

	if(c == '\\')
		c1 = *p;
	else
		c1 = c;

	while(true){
		if((c == '[' || *t == c1) && Com_GlobMatch(p - 1, t))
			return 1;
		if(*t++ == '\0')
			return 0;
	}
}


/* Match the pattern against the text;
   return 1 if it matches, 0 otherwise.

   A match means the entire string TEXT is used up in matching.

   In the pattern string, `*' matches any sequence of characters,
   `?' matches any character, [SET] matches any character in the specified set,
   [!SET] matches any character not in the specified set.

   A set is composed of characters or ranges; a range looks like
   character hyphen character(as in 0-9 or A-Z).
   [0-9a-zA-Z_] is the set of characters allowed in C identifiers.
   Any other character in the pattern must be matched exactly.

   To suppress the special syntactic significance of any of `[]*?!-\',
   and match the character exactly, precede it with a `\'.
*/

int Com_GlobMatch(const char *pattern, const char *text){
	const char *p = pattern, *t = text;
	register char c;

	while((c = *p++) != '\0')
		switch(c){
			case '?':
				if(*t == '\0')
					return 0;
				else
					++t;
				break;

			case '\\':
				if(*p++ != *t++)
					return 0;
				break;

			case '*':
				return Com_GlobMatchStar(p, t);

			case '[': {
					register char c1 = *t++;
					int invert;

					if(!c1)
						return(0);

					invert = ((*p == '!') || (*p == '^'));
					if(invert)
						p++;

					c = *p++;
					while(true){
						register char cstart = c, cend = c;

						if(c == '\\'){
							cstart = *p++;
							cend = cstart;
						}
						if(c == '\0')
							return 0;

						c = *p++;
						if(c == '-' && *p != ']'){
							cend = *p++;
							if(cend == '\\')
								cend = *p++;
							if(cend == '\0')
								return 0;
							c = *p++;
						}
						if(c1 >= cstart && c1 <= cend)
							goto match;
						if(c == ']')
							break;
					}
					if(!invert)
						return 0;
					break;

match:
					/* Skip the rest of the [...] construct that already matched.  */
					while(c != ']'){
						if(c == '\0')
							return 0;
						c = *p++;
						if(c == '\0')
							return 0;
						else if(c == '\\')
							++p;
					}
					if(invert)
						return 0;
					break;
				}

			default:
				if(c != *t++)
					return 0;
		}

	return *t == '\0';
}


/*
 * Com_Basename
 */
const char *Com_Basename(const char *path){
	const char *last;

	last = path;
	while(*path){
		if(*path == '/')
			last = path + 1;
		path++;
	}
	return last;
}


/*
 * Com_Dirname
 */
void Com_Dirname(const char *in, char *out){
	char *c;

	if(!(c = strrchr(in, '/'))){
		strcpy(out, "./");
		return;
	}

	strncpy(out, in, (c - in) + 1);
	out[(c - in) + 1] = 0;
}


/*
 * Com_StripExtension
 */
void Com_StripExtension(const char *in, char *out){
	while(*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}


/*
 * Com_StripColor
 */
void Com_StripColor(const char *in, char *out){

	while(*in){

		if(IS_COLOR(in)){
			in += 2;
			continue;
		}

		if(IS_LEGACY_COLOR(in)){
			in++;
			continue;
		}

		*out++ = *in++;
	}
	*out = 0;
}


/*
 * Com_TrimString
 */
char *Com_TrimString(char *in){
	char *c;

	while(*in == ' ' || *in == '\t')
		in++;

	c = &in[strlen(in) - 1];

	while(*c == ' ' || *c == '\t'){
		*c = 0;
		c--;
	}

	return in;
}


/*
 * BYTE ORDER FUNCTIONS
 */

static qboolean bigendien;

// can't just use function pointers, or dll linkage can
// mess up when common is included in multiple places
short (*_BigShort)(short l);
short (*_LittleShort)(short l);
int (*_BigLong)(int l);
int (*_LittleLong)(int l);
float (*_BigFloat)(float l);
float (*_LittleFloat)(float l);

short BigShort(short l){
	return _BigShort(l);
}
short LittleShort(short l){
	return _LittleShort(l);
}
int BigLong(int l){
	return _BigLong(l);
}
int LittleLong(int l){
	return _LittleLong(l);
}
float BigFloat(float l){
	return _BigFloat(l);
}
float LittleFloat(float l){
	return _LittleFloat(l);
}

static short ShortSwap(short l){
	byte b1, b2;

	b1 = l & 255;
	b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

static short ShortNoSwap(short l){
	return l;
}

static int LongSwap(int l){
	byte b1, b2, b3, b4;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

static int LongNoSwap(int l){
	return l;
}

static float FloatSwap(float f){
	union {
		float f;
		byte b[4];
	} dat1, dat2;

	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

static float FloatNoSwap(float f){
	return f;
}

/*
 * Swap_Init
 */
void Swap_Init(void){

	static union {
		byte b[2];
		unsigned short s;
	} swaptest;

	swaptest.b[0] = 1;
	swaptest.b[1] = 0;

	// set the byte swapping variables in a portable manner
	if(swaptest.s == 1){
		bigendien = false;
		_BigShort = ShortSwap;
		_LittleShort = ShortNoSwap;
		_BigLong = LongSwap;
		_LittleLong = LongNoSwap;
		_BigFloat = FloatSwap;
		_LittleFloat = FloatNoSwap;
	} else {
		bigendien = true;
		_BigShort = ShortNoSwap;
		_LittleShort = ShortSwap;
		_BigLong = LongNoSwap;
		_LittleLong = LongSwap;
		_BigFloat = FloatNoSwap;
		_LittleFloat = FloatSwap;
	}
}


/*
 * va
 *
 * A shorthand sprintf into a temp buffer.
 */
char *va(const char *format, ...){
	va_list	argptr;
	static char string[MAX_STRING_CHARS];

	memset(string, 0, sizeof(string));

	va_start(argptr, format);
	vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	return string;
}


char com_token[MAX_TOKEN_CHARS];

/*
 * Com_Parse
 *
 * Parse a token out of a string
 */
char *Com_Parse(const char **data_p){
	int c;
	int len;
	const char *data;

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	if(!data){
		*data_p = NULL;
		return "";
	}

	// skip whitespace
skipwhite:
	while((c = *data) <= ' '){
		if(c == 0){
			*data_p = NULL;
			return "";
		}
		data++;
	}

	// skip // comments
	if(c == '/' && data[1] == '/'){
		while(*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if(c == '\"'){
		data++;
		while(true){
			c = *data++;
			if(c == '\"' || !c){
				com_token[len] = 0;
				*data_p = data;
				return com_token;
			}
			if(len < MAX_TOKEN_CHARS){
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do {
		if(len < MAX_TOKEN_CHARS){
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while(c > 32);

	if(len == MAX_TOKEN_CHARS){
		len = 0;
	}
	com_token[len] = 0;

	*data_p = data;
	return com_token;
}


/*
 * INFO STRINGS
 */

/*
 * Info_ValueForKey
 *
 * Searches the string for the given
 * key and returns the associated value, or an empty string.
 */
char *Info_ValueForKey(const char *s, const char *key){
	char pkey[512];
	static char value[2][512];  // use two buffers so compares
	// work without stomping on each other
	static int valueindex;
	char *o;

	valueindex ^= 1;
	if(*s == '\\')
		s++;
	while(true){
		o = pkey;
		while(*s != '\\'){
			if(!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while(*s != '\\' && *s){
			if(!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if(!strcmp(key, pkey))
			return value[valueindex];

		if(!*s)
			return "";
		s++;
	}
}


/*
 * Info_RemoveKey
 */
void Info_RemoveKey(char *s, const char *key){
	char *start;
	char pkey[512];
	char value[512];
	char *o;

	if(strstr(key, "\\")){
		return;
	}

	while(true){
		start = s;
		if(*s == '\\')
			s++;
		o = pkey;
		while(*s != '\\'){
			if(!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while(*s != '\\' && *s){
			if(!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if(!strcmp(key, pkey)){
			strcpy(start, s);  // remove this part
			return;
		}

		if(!*s)
			return;
	}
}


/*
 * Info_Validate
 *
 * Some characters are illegal in info strings because they
 * can mess up the server's parsing
 */
qboolean Info_Validate(const char *s){
	if(strstr(s, "\""))
		return false;
	if(strstr(s, ";"))
		return false;
	return true;
}


/*
 * Info_SetValueForKey
 */
void Info_SetValueForKey(char *s, const char *key, const char *value){
	char newi[MAX_INFO_STRING], *v;
	int c;
	int maxsize = MAX_INFO_STRING;

	if(strstr(key, "\\") || strstr(value, "\\")){
		Com_Printf("Can't use keys or values with a \\\n");
		return;
	}

	if(strstr(key, ";")){
		Com_Printf("Can't use keys or values with a semicolon\n");
		return;
	}

	if(strstr(key, "\"") || strstr(value, "\"")){
		Com_Printf("Can't use keys or values with a \"\n");
		return;
	}

	if(strlen(key) > MAX_INFO_KEY - 1 || strlen(value) > MAX_INFO_KEY - 1){
		Com_Printf("Keys and values must be < 64 characters.\n");
		return;
	}
	Info_RemoveKey(s, key);
	if(!value || !strlen(value))
		return;

	snprintf(newi, sizeof(newi), "\\%s\\%s", key, value);

	if(strlen(newi) + strlen(s) > maxsize){
		Com_Printf("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newi;
	while(*v){
		c = *v++;
		c &= 127;  // strip high bits
		if(c >= 32 && c < 127)
			*s++ = c;
	}
	*s = 0;
}


/*
 * ColorByName
 */
int ColorByName(const char *s, int def){
	int i;

	if(!s || !strlen(s))
		return def;

	i = atoi(s);
	if(i > 0 && i < 255)
		return i;

	if(!strcasecmp(s, "red"))
		return 242;
	if(!strcasecmp(s, "green"))
		return 209;
	if(!strcasecmp(s, "blue"))
		return 243;
	if(!strcasecmp(s, "yellow"))
		return 219;
	if(!strcasecmp(s, "orange"))
		return 225;
	if(!strcasecmp(s, "white"))
		return 216;
	if(!strcasecmp(s, "pink"))
		return 247;
	if(!strcasecmp(s, "purple"))
		return 187;
	return def;
}
