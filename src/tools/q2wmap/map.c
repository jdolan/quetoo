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

#include "qbsp.h"
#include "scriplib.h"

int nummapbrushes;
mapbrush_t mapbrushes[MAX_BSP_BRUSHES];

#define MAX_BSP_SIDES (MAX_BSP_BRUSHES * 6)

static int nummapbrushsides;
static side_t brushsides[MAX_BSP_SIDES];
static brush_texture_t side_brushtextures[MAX_BSP_SIDES];

int nummapplanes;
plane_t mapplanes[MAX_BSP_PLANES];

#define	PLANE_HASHES 1024
static plane_t *planehash[PLANE_HASHES];

vec3_t map_mins, map_maxs;

static int c_boxbevels;
static int c_edgebevels;
static int c_areaportals;
static int c_clipbrushes;


/*
 * PlaneTypeForNormal
 *
 * Set the type of the plane according to it's normal vector
 */
static int PlaneTypeForNormal(const vec3_t normal){
	vec_t ax, ay, az;

	// NOTE: should these have an epsilon around 1.0?
	if(normal[0] == 1.0 || normal[0] == -1.0)
		return PLANE_X;
	if(normal[1] == 1.0 || normal[1] == -1.0)
		return PLANE_Y;
	if(normal[2] == 1.0 || normal[2] == -1.0)
		return PLANE_Z;

	ax = fabs(normal[0]);
	ay = fabs(normal[1]);
	az = fabs(normal[2]);

	if(ax >= ay && ax >= az)
		return PLANE_ANYX;
	if(ay >= ax && ay >= az)
		return PLANE_ANYY;
	return PLANE_ANYZ;
}


/*
 * PlaneEqual
 */
#define	NORMAL_EPSILON	0.00001
#define	DIST_EPSILON	0.01
static inline qboolean PlaneEqual(const plane_t * p, const vec3_t normal, const vec_t dist){
	if(fabs(p->normal[0] - normal[0]) < NORMAL_EPSILON
	        && fabs(p->normal[1] - normal[1]) < NORMAL_EPSILON
	        && fabs(p->normal[2] - normal[2]) < NORMAL_EPSILON
	        && fabs(p->dist - dist) < DIST_EPSILON)
		return true;
	return false;
}


/*
 * AddPlaneToHash
 */
static inline void AddPlaneToHash(plane_t * p){
	int hash;

	hash = (int)fabs(p->dist) / 8;
	hash &= (PLANE_HASHES - 1);

	p->hash_chain = planehash[hash];
	planehash[hash] = p;
}


/*
 * CreateNewFloatPlane
 */
static int CreateNewFloatPlane(vec3_t normal, vec_t dist){
	plane_t *p, temp;

	if(VectorLength(normal) < 0.5)
		Error("FloatPlane: bad normal\n");
	// create a new plane
	if(nummapplanes + 2 > MAX_BSP_PLANES)
		Error("MAX_BSP_PLANES\n");

	p = &mapplanes[nummapplanes];
	VectorCopy(normal, p->normal);
	p->dist = dist;
	p->type = (p + 1)->type = PlaneTypeForNormal(p->normal);

	VectorSubtract(vec3_origin, normal, (p + 1)->normal);
	(p + 1)->dist = -dist;

	nummapplanes += 2;

	// always put axial planes facing positive first
	if(AXIAL(p)){
		if(p->normal[0] < 0 || p->normal[1] < 0 || p->normal[2] < 0){
			// flip order
			temp = *p;
			*p = *(p + 1);
			*(p + 1) = temp;

			AddPlaneToHash(p);
			AddPlaneToHash(p + 1);
			return nummapplanes - 1;
		}
	}

	AddPlaneToHash(p);
	AddPlaneToHash(p + 1);
	return nummapplanes - 2;
}


/*
 * SnapVector
 */
static void SnapVector(vec3_t normal){
	int i;

	for(i = 0; i < 3; i++){
		if(fabs(normal[i] - 1) < NORMAL_EPSILON){
			VectorClear(normal);
			normal[i] = 1;
			break;
		}
		if(fabs(normal[i] - -1) < NORMAL_EPSILON){
			VectorClear(normal);
			normal[i] = -1;
			break;
		}
	}
}


/*
 * SnapPlane
 */
static inline void SnapPlane(vec3_t normal, vec_t *dist){
	const float f = floor(*dist + 0.5);

	SnapVector(normal);

	if(fabs(*dist - f) < DIST_EPSILON)
		*dist = f;
}


/*
 * FindFloatPlane
 */
int FindFloatPlane(vec3_t normal, vec_t dist){
	int i;
	const plane_t *p;
	int hash;

	SnapPlane(normal, &dist);
	hash = (int)fabs(dist) / 8;
	hash &= (PLANE_HASHES - 1);

	// search the border bins as well
	for(i = -1; i <= 1; i++){
		const int h = (hash + i) & (PLANE_HASHES - 1);
		for(p = planehash[h]; p; p = p->hash_chain){
			if(PlaneEqual(p, normal, dist))
				return p - mapplanes;
		}
	}

	return CreateNewFloatPlane(normal, dist);
}


/*
 * PlaneFromPoints
 */
static int PlaneFromPoints(const vec3_t p0, const vec3_t p1, const vec3_t p2){
	vec3_t t1, t2, normal;
	vec_t dist;

	VectorSubtract(p0, p1, t1);
	VectorSubtract(p2, p1, t2);
	CrossProduct(t1, t2, normal);
	VectorNormalize(normal);

	dist = DotProduct(p0, normal);

	return FindFloatPlane(normal, dist);
}


/*
 * BrushContents
 */
static int BrushContents(const mapbrush_t * b){
	int contents;
	const side_t *s;
	int i;
	int trans;

	s = &b->original_sides[0];
	contents = s->contents;
	trans = texinfo[s->texinfo].flags;
	for(i = 1; i < b->numsides; i++, s++){
		trans |= texinfo[s->texinfo].flags;
		if(s->contents != contents){
			Verbose("Entity %i, Brush %i: mixed face contents\n", b->entitynum,
			           b->brushnum);
			break;
		}
	}

	// if any side is translucent, mark the contents and change solid to window
	if(trans & (SURF_ALPHATEST | SURF_BLEND33 | SURF_BLEND66)){
		contents |= CONTENTS_TRANSLUCENT;
		if(contents & CONTENTS_SOLID){
			contents &= ~CONTENTS_SOLID;
			contents |= CONTENTS_WINDOW;
		}
	}

	return contents;
}


/*
 * AddBrushBevels
 *
 * Adds any additional planes necessary to allow the brush to be expanded
 * against axial bounding boxes
 */
static void AddBrushBevels(mapbrush_t * b){
	int axis, dir;
	int i, j, k, l, order;
	side_t sidetemp;
	brush_texture_t tdtemp;
	side_t *s, *s2;
	vec3_t normal;
	float dist;
	winding_t *w, *w2;
	vec3_t vec, vec2;
	float d;

	// add the axial planes
	order = 0;
	for(axis = 0; axis < 3; axis++){
		for(dir = -1; dir <= 1; dir += 2, order++){
			// see if the plane is already present
			for(i = 0, s = b->original_sides; i < b->numsides; i++, s++){
				if(mapplanes[s->planenum].normal[axis] == dir)
					break;
			}

			if(i == b->numsides){ // add a new side
				if(nummapbrushsides == MAX_BSP_BRUSHSIDES)
					Error("MAX_BSP_BRUSHSIDES\n");
				nummapbrushsides++;
				b->numsides++;
				VectorClear(normal);
				normal[axis] = dir;
				if(dir == 1)
					dist = b->maxs[axis];
				else
					dist = -b->mins[axis];
				s->planenum = FindFloatPlane(normal, dist);
				s->texinfo = b->original_sides[0].texinfo;
				s->contents = b->original_sides[0].contents;
				s->bevel = true;
				c_boxbevels++;
			}
			// if the plane is not in it canonical order, swap it
			if(i != order){
				sidetemp = b->original_sides[order];
				b->original_sides[order] = b->original_sides[i];
				b->original_sides[i] = sidetemp;

				j = b->original_sides - brushsides;
				tdtemp = side_brushtextures[j + order];
				side_brushtextures[j + order] = side_brushtextures[j + i];
				side_brushtextures[j + i] = tdtemp;
			}
		}
	}

	// add the edge bevels
	if(b->numsides == 6)
		return;  // pure axial

	// test the non-axial plane edges
	for(i = 6; i < b->numsides; i++){
		s = b->original_sides + i;
		w = s->winding;
		if(!w)
			continue;
		for(j = 0; j < w->numpoints; j++){
			k = (j + 1) % w->numpoints;
			VectorSubtract(w->p[j], w->p[k], vec);
			if(VectorNormalize(vec) < 0.5)
				continue;
			SnapVector(vec);
			for(k = 0; k < 3; k++)
				if(vec[k] == -1 || vec[k] == 1)
					break;			  // axial
			if(k != 3)
				continue;			  // only test non-axial edges

			// try the six possible slanted axials from this edge
			for(axis = 0; axis < 3; axis++){
				for(dir = -1; dir <= 1; dir += 2){
					// construct a plane
					VectorClear(vec2);
					vec2[axis] = dir;
					CrossProduct(vec, vec2, normal);
					if(VectorNormalize(normal) < 0.5)
						continue;
					dist = DotProduct(w->p[j], normal);

					// if all the points on all the sides are
					// behind this plane, it is a proper edge bevel
					for(k = 0; k < b->numsides; k++){
						float minBack;

						// if this plane has already been used, skip it
						if(PlaneEqual(&mapplanes[b->original_sides[k].planenum]
						               , normal, dist))
							break;

						w2 = b->original_sides[k].winding;
						if(!w2)
							continue;
						minBack = 0.0f;
						for(l = 0; l < w2->numpoints; l++){
							d = DotProduct(w2->p[l], normal) - dist;
							if(d > 0.1)
								break;  // point in front
							if(d < minBack)
								minBack = d;
						}
						// if some point was at the front
						if(l != w2->numpoints)
							break;
						// if no points at the back then the winding is on the
						// bevel plane
						if(minBack > -0.1f)
							break;
					}

					if(k != b->numsides)
						continue;	  // wasn't part of the outer hull
					// add this plane
					if(nummapbrushsides == MAX_BSP_BRUSHSIDES)
						Error("MAX_BSP_BRUSHSIDES\n");
					nummapbrushsides++;
					s2 = &b->original_sides[b->numsides];
					s2->planenum = FindFloatPlane(normal, dist);
					s2->texinfo = b->original_sides[0].texinfo;
					s2->contents = b->original_sides[0].contents;
					s2->bevel = true;
					c_edgebevels++;
					b->numsides++;
				}
			}
		}
	}
}


/*
 * MakeBrushWindings
 *
 * Makes basewindigs for sides and mins / maxs for the brush
 */
static qboolean MakeBrushWindings(mapbrush_t * ob){
	int i, j;
	side_t *side;

	ClearBounds(ob->mins, ob->maxs);

	for(i = 0; i < ob->numsides; i++){
		const plane_t *plane = &mapplanes[ob->original_sides[i].planenum];
		winding_t *w = BaseWindingForPlane(plane->normal, plane->dist);
		for(j = 0; j < ob->numsides && w; j++){
			if(i == j)
				continue;
			// back side clipaway
			if(ob->original_sides[j].planenum == (ob->original_sides[j].planenum ^ 1))
				continue;
			if(ob->original_sides[j].bevel)
				continue;
			plane = &mapplanes[ob->original_sides[j].planenum ^ 1];
			ChopWindingInPlace(&w, plane->normal, plane->dist, 0);	//CLIP_EPSILON);
		}

		side = &ob->original_sides[i];
		side->winding = w;
		if(w){
			side->visible = true;
			for(j = 0; j < w->numpoints; j++)
				AddPointToBounds(w->p[j], ob->mins, ob->maxs);
		}
	}

	for(i = 0; i < 3; i++){
		if(ob->mins[0] < -MAX_WORLD_WIDTH || ob->maxs[0] > MAX_WORLD_WIDTH)
			Verbose("entity %i, brush %i: bounds out of range\n", ob->entitynum,
			           ob->brushnum);
		if(ob->mins[0] > MAX_WORLD_WIDTH || ob->maxs[0] < -MAX_WORLD_WIDTH)
			Verbose("entity %i, brush %i: no visible sides on brush\n",
			           ob->entitynum, ob->brushnum);
	}

	return true;
}


/*
 * SetImpliedFlags
 */
static void SetImpliedFlags(side_t *side, const char *tex){

	if(!strcmp(tex, "common/caulk"))
		side->surf |= SURF_NODRAW;
	else if(!strcmp(tex, "common/trigger"))
		side->surf |= SURF_NODRAW;
	else if(!strcmp(tex, "common/sky"))
		side->surf |= SURF_SKY;
	else if(!strcmp(tex, "common/hint"))
		side->surf |= SURF_HINT;
	else if(!strcmp(tex, "common/clip"))
		side->contents |= CONTENTS_PLAYERCLIP;
	else if(!strcmp(tex, "common/ladder"))
		side->contents |= CONTENTS_LADDER;

	if(strstr(tex, "lava")){
		side->surf |= SURF_WARP;
		side->contents |= CONTENTS_LAVA;
	}

	if(strstr(tex, "slime")){
		side->surf |= SURF_WARP;
		side->contents |= CONTENTS_SLIME;
	}

	if(strstr(tex, "water")){
		side->surf |= SURF_WARP;
		side->contents |= CONTENTS_WATER;
	}
}


/*
 * ParseBrush
 */
static void ParseBrush(entity_t *mapent){
	mapbrush_t *b;
	int i, j, k;
	side_t *side, *s2;
	int planenum;
	brush_texture_t td;
	vec3_t planepts[3];

	if(nummapbrushes == MAX_BSP_BRUSHES)
		Error("nummapbrushes == MAX_BSP_BRUSHES\n");

	b = &mapbrushes[nummapbrushes];
	b->original_sides = &brushsides[nummapbrushsides];
	b->entitynum = num_entities - 1;
	b->brushnum = nummapbrushes - mapent->firstbrush;

	do {
		if(!GetToken(true))
			break;
		if(!strcmp(token, "}"))
			break;

		if(nummapbrushsides == MAX_BSP_BRUSHSIDES)
			Error("MAX_BSP_BRUSHSIDES\n");
		side = &brushsides[nummapbrushsides];

		// read the three point plane definition
		for(i = 0; i < 3; i++){
			if(i != 0)
				GetToken(true);
			if(strcmp(token, "("))
				Error("Parsing brush\n");

			for(j = 0; j < 3; j++){
				GetToken(false);
				planepts[i][j] = atof(token);
			}

			GetToken(false);
			if(strcmp(token, ")"))
				Error("Parsing brush\n");
		}

		memset(&td, 0, sizeof(td));

		// read the texturedef
		GetToken(false);

		if(strlen(token) > sizeof(td.name) - 1)
			Error("Texture name \"%s\" is too long.\n", token);

		strncpy(td.name, token, sizeof(td.name) - 1);

		GetToken(false);
		td.shift[0] = atoi(token);
		GetToken(false);
		td.shift[1] = atoi(token);
		GetToken(false);
		td.rotate = atoi(token);
		GetToken(false);
		td.scale[0] = atof(token);
		GetToken(false);
		td.scale[1] = atof(token);

		if(TokenAvailable()){
			GetToken(false);
			side->contents = atoi(token);
			GetToken(false);
			side->surf = td.flags = atoi(token);
			GetToken(false);
			td.value = atoi(token);
		}
		else {
			side->contents = CONTENTS_SOLID;
			side->surf = td.flags = 0;
			td.value = 0;
		}

		// resolve implicit surface and contents flags
		SetImpliedFlags(side, td.name);

		// translucent objects are automatically classified as detail
		if(side->surf & (SURF_ALPHATEST | SURF_BLEND33 | SURF_BLEND66))
			side->contents |= CONTENTS_DETAIL;
		if(side->contents & (CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP))
			side->contents |= CONTENTS_DETAIL;
		if(fulldetail)
			side->contents &= ~CONTENTS_DETAIL;
		if(!(side->contents & ((LAST_VISIBLE_CONTENTS - 1)
					| CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP | CONTENTS_MIST)))
			side->contents |= CONTENTS_SOLID;

		// hints and skips are never detail, and have no content
		if(side->surf & (SURF_HINT | SURF_SKIP)){
			side->contents = 0;
			side->surf &= ~CONTENTS_DETAIL;
		}

		// find the plane number
		planenum = PlaneFromPoints(planepts[0], planepts[1], planepts[2]);
		if(planenum == -1){
			Verbose("Entity %i, Brush %i: plane with no normal\n", b->entitynum,
			           b->brushnum);
			continue;
		}

		// see if the plane has been used already
		for(k = 0; k < b->numsides; k++){
			s2 = b->original_sides + k;
			if(s2->planenum == planenum){
				Verbose("Entity %i, Brush %i: duplicate plane\n", b->entitynum,
				           b->brushnum);
				break;
			}
			if(s2->planenum == (planenum ^ 1)){
				Verbose("Entity %i, Brush %i: mirrored plane\n", b->entitynum,
				           b->brushnum);
				break;
			}
		}
		if(k != b->numsides)
			continue;  // duplicated

		// keep this side
		side = b->original_sides + b->numsides;
		side->planenum = planenum;
		side->texinfo = TexinfoForBrushTexture(&mapplanes[planenum], &td, vec3_origin);

		// save the td off in case there is an origin brush and we
		// have to recalculate the texinfo
		side_brushtextures[nummapbrushsides] = td;

		nummapbrushsides++;
		b->numsides++;
	} while(true);

	// get the content for the entire brush
	b->contents = BrushContents(b);

	// allow detail brushes to be removed
	if(nodetail && (b->contents & CONTENTS_DETAIL)){
		b->numsides = 0;
		return;
	}

	// allow water brushes to be removed
	if(nowater &&
	        (b->contents & (CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER))){
		b->numsides = 0;
		return;
	}

	// create windings for sides and bounds for brush
	MakeBrushWindings(b);

	// brushes that will not be visible at all will never be
	// used as bsp splitters
	if(b->contents & (CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP)){
		c_clipbrushes++;
		for(i = 0; i < b->numsides; i++)
			b->original_sides[i].texinfo = TEXINFO_NODE;
	}

	// origin brushes are removed, but they set
	// the rotation origin for the rest of the brushes
	// in the entity.  After the entire entity is parsed,
	// the planenums and texinfos will be adjusted for
	// the origin brush
	if(b->contents & CONTENTS_ORIGIN){
		char string[32];
		vec3_t origin;

		if(num_entities == 1){
			Error("Entity %i, Brush %i: origin brushes not allowed in world\n",
			      b->entitynum, b->brushnum);
			return;
		}

		VectorAdd(b->mins, b->maxs, origin);
		VectorScale(origin, 0.5, origin);

		sprintf(string, "%i %i %i", (int)origin[0], (int)origin[1],
		        (int)origin[2]);
		SetKeyValue(&entities[b->entitynum], "origin", string);

		VectorCopy(origin, entities[b->entitynum].origin);

		// don't keep this brush
		b->numsides = 0;

		return;
	}

	AddBrushBevels(b);

	nummapbrushes++;
	mapent->numbrushes++;
}

/*
 * MoveBrushesToWorld
 *
 * Takes all of the brushes from the current entity and
 * adds them to the world's brush list.
 *
 * Used by func_group and func_areaportal
 */
static void MoveBrushesToWorld(entity_t * mapent){
	int newbrushes;
	int worldbrushes;
	mapbrush_t *temp;
	int i;

	// this is pretty gross, because the brushes are expected to be
	// in linear order for each entity

	newbrushes = mapent->numbrushes;
	worldbrushes = entities[0].numbrushes;

	temp = Z_Malloc(newbrushes * sizeof(mapbrush_t));
	memcpy(temp, mapbrushes + mapent->firstbrush,
	       newbrushes * sizeof(mapbrush_t));

	// make space to move the brushes (overlapped copy)
	memmove(mapbrushes + worldbrushes + newbrushes,
	        mapbrushes + worldbrushes,
	        sizeof(mapbrush_t) * (nummapbrushes - worldbrushes - newbrushes));

	// copy the new brushes down
	memcpy(mapbrushes + worldbrushes, temp, sizeof(mapbrush_t) * newbrushes);

	// fix up indexes
	entities[0].numbrushes += newbrushes;
	for(i = 1; i < num_entities; i++)
		entities[i].firstbrush += newbrushes;
	Z_Free(temp);

	mapent->numbrushes = 0;
}


/*
 * ParseMapEntity
 */
static qboolean ParseMapEntity(void){
	entity_t *mapent;
	epair_t *e;
	side_t *s;
	int i, j;
	vec_t newdist;
	mapbrush_t *b;

	if(!GetToken(true))
		return false;

	if(strcmp(token, "{"))
		Error("ParseMapEntity: { not found\n");

	if(num_entities == MAX_BSP_ENTITIES)
		Error("num_entities == MAX_BSP_ENTITIES\n");

	mapent = &entities[num_entities];
	num_entities++;
	memset(mapent, 0, sizeof(*mapent));
	mapent->firstbrush = nummapbrushes;
	mapent->numbrushes = 0;

	do {
		if(!GetToken(true))
			Error("ParseMapEntity: EOF without closing brace\n");
		if(!strcmp(token, "}"))
			break;
		if(!strcmp(token, "{"))
			ParseBrush(mapent);
		else {
			e = ParseEpair();
			e->next = mapent->epairs;
			mapent->epairs = e;
		}
	} while(true);

	GetVectorForKey(mapent, "origin", mapent->origin);

	// if there was an origin brush, offset all of the planes and texinfo
	if(mapent->origin[0] || mapent->origin[1] || mapent->origin[2]){
		for(i = 0; i < mapent->numbrushes; i++){
			b = &mapbrushes[mapent->firstbrush + i];
			for(j = 0; j < b->numsides; j++){

				s = &b->original_sides[j];

				newdist = mapplanes[s->planenum].dist -
				          DotProduct(mapplanes[s->planenum].normal, mapent->origin);

				s->planenum = FindFloatPlane(mapplanes[s->planenum].normal, newdist);

				s->texinfo = TexinfoForBrushTexture(&mapplanes[s->planenum],
						&side_brushtextures[s - brushsides], mapent->origin);
			}
			MakeBrushWindings(b);
		}
	}

	// group entities are just for editor convenience
	// toss all brushes into the world entity
	if(!strcmp("func_group", ValueForKey(mapent, "classname"))){
		MoveBrushesToWorld(mapent);
		mapent->numbrushes = 0;
		return true;
	}

	// areaportal entities move their brushes, but don't eliminate the entity
	if(!strcmp("func_areaportal", ValueForKey(mapent, "classname"))){
		char str[128];

		if(mapent->numbrushes != 1)
			Error("ParseMapEntity: %i func_areaportal can only be a single brush\n",
			      num_entities - 1);

		b = &mapbrushes[nummapbrushes - 1];
		b->contents = CONTENTS_AREAPORTAL;
		c_areaportals++;
		mapent->areaportalnum = c_areaportals;
		// set the portal number as "style"
		sprintf(str, "%i", c_areaportals);
		SetKeyValue(mapent, "style", str);
		MoveBrushesToWorld(mapent);
		return true;
	}

	return true;
}


/*
 * LoadMapFile
 */
void LoadMapFile(const char *filename){
	int subdivide;
	int i;

	Verbose("--- LoadMapFile ---\n");

	LoadScriptFile(filename);

	memset(mapbrushes, 0, sizeof(mapbrush_t) * MAX_BSP_BRUSHES);
	nummapbrushes = 0;

	memset(brushsides, 0, sizeof(side_t) * MAX_BSP_SIDES);
	nummapbrushsides = 0;

	memset(side_brushtextures, 0, sizeof(brush_texture_t) * MAX_BSP_SIDES);

	memset(mapplanes, 0, sizeof(plane_t) * MAX_BSP_PLANES);
	numplanes = 0;

	num_entities = 0;
	numtexinfo = 0;

	while(ParseMapEntity()){}

	subdivide = atoi(ValueForKey(&entities[0], "subdivide"));

	if(subdivide >= 256 && subdivide <= 2048){
		Verbose("Using subdivide %d from worldspawn.\n", subdivide);
		subdivide_size = subdivide;
	}

	ClearBounds(map_mins, map_maxs);
	for(i = 0; i < entities[0].numbrushes; i++){
		if(mapbrushes[i].mins[0] > MAX_WORLD_WIDTH)
			continue;  // no valid points
		AddPointToBounds(mapbrushes[i].mins, map_mins, map_maxs);
		AddPointToBounds(mapbrushes[i].maxs, map_mins, map_maxs);
	}

	Verbose("%5i brushes\n", nummapbrushes);
	Verbose("%5i clipbrushes\n", c_clipbrushes);
	Verbose("%5i total sides\n", nummapbrushsides);
	Verbose("%5i boxbevels\n", c_boxbevels);
	Verbose("%5i edgebevels\n", c_edgebevels);
	Verbose("%5i entities\n", num_entities);
	Verbose("%5i planes\n", nummapplanes);
	Verbose("%5i areaportals\n", c_areaportals);
	Verbose("size: %5.0f,%5.0f,%5.0f to %5.0f,%5.0f,%5.0f\n",
			map_mins[0], map_mins[1], map_mins[2], map_maxs[0], map_maxs[1], map_maxs[2]);
}
