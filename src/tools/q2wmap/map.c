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

int num_map_brushes;
map_brush_t map_brushes[MAX_BSP_BRUSHES];

#define MAX_BSP_SIDES (MAX_BSP_BRUSHES * 6)

static int num_map_brush_sides;
static side_t map_brush_sides[MAX_BSP_SIDES];
static map_brush_texture_t map_brush_textures[MAX_BSP_SIDES];

int num_map_planes;
map_plane_t map_planes[MAX_BSP_PLANES];

#define	PLANE_HASHES 1024
static map_plane_t *plane_hash[PLANE_HASHES];

vec3_t map_mins, map_maxs;

static int c_box_bevels;
static int c_edge_bevels;
static int c_area_portals;
static int c_clip_brushes;

/*
 * PlaneTypeForNormal
 *
 * Set the type of the plane according to it's normal vector
 */
static int PlaneTypeForNormal(const vec3_t normal) {
	vec_t ax, ay, az;

	// NOTE: should these have an epsilon around 1.0?
	if (normal[0] == 1.0 || normal[0] == -1.0)
		return PLANE_X;
	if (normal[1] == 1.0 || normal[1] == -1.0)
		return PLANE_Y;
	if (normal[2] == 1.0 || normal[2] == -1.0)
		return PLANE_Z;

	ax = fabs(normal[0]);
	ay = fabs(normal[1]);
	az = fabs(normal[2]);

	if (ax >= ay && ax >= az)
		return PLANE_ANYX;
	if (ay >= ax && ay >= az)
		return PLANE_ANYY;
	return PLANE_ANYZ;
}

/*
 * PlaneEqual
 */
#define	NORMAL_EPSILON	0.00001
#define	DIST_EPSILON	0.01
static inline boolean_t PlaneEqual(const map_plane_t * p, const vec3_t normal,
		const vec_t dist) {
	if (fabs(p->normal[0] - normal[0]) < NORMAL_EPSILON && fabs(
			p->normal[1] - normal[1]) < NORMAL_EPSILON && fabs(
			p->normal[2] - normal[2]) < NORMAL_EPSILON && fabs(p->dist - dist)
			< DIST_EPSILON)
		return true;
	return false;
}

/*
 * AddPlaneToHash
 */
static inline void AddPlaneToHash(map_plane_t * p) {
	int hash;

	hash = (int) fabs(p->dist) / 8;
	hash &= (PLANE_HASHES - 1);

	p->hash_chain = plane_hash[hash];
	plane_hash[hash] = p;
}

/*
 * CreateNewFloatPlane
 */
static int CreateNewFloatPlane(vec3_t normal, vec_t dist) {
	map_plane_t *p, temp;

	if (VectorLength(normal) < 0.5)
		Com_Error(ERR_FATAL, "FloatPlane: bad normal\n");
	// create a new plane
	if (num_map_planes + 2 > MAX_BSP_PLANES)
		Com_Error(ERR_FATAL, "MAX_BSP_PLANES\n");

	p = &map_planes[num_map_planes];
	VectorCopy(normal, p->normal);
	p->dist = dist;
	p->type = (p + 1)->type = PlaneTypeForNormal(p->normal);

	VectorSubtract(vec3_origin, normal, (p + 1)->normal);
	(p + 1)->dist = -dist;

	num_map_planes += 2;

	// always put axial planes facing positive first
	if (AXIAL(p)) {
		if (p->normal[0] < 0 || p->normal[1] < 0 || p->normal[2] < 0) {
			// flip order
			temp = *p;
			*p = *(p + 1);
			*(p + 1) = temp;

			AddPlaneToHash(p);
			AddPlaneToHash(p + 1);
			return num_map_planes - 1;
		}
	}

	AddPlaneToHash(p);
	AddPlaneToHash(p + 1);
	return num_map_planes - 2;
}

/*
 * SnapVector
 */
static void SnapVector(vec3_t normal) {
	int i;

	for (i = 0; i < 3; i++) {
		if (fabs(normal[i] - 1) < NORMAL_EPSILON) {
			VectorClear(normal);
			normal[i] = 1;
			break;
		}
		if (fabs(normal[i] - -1) < NORMAL_EPSILON) {
			VectorClear(normal);
			normal[i] = -1;
			break;
		}
	}
}

/*
 * SnapPlane
 */
static inline void SnapPlane(vec3_t normal, vec_t *dist) {
	const float f = floor(*dist + 0.5);

	SnapVector(normal);

	if (fabs(*dist - f) < DIST_EPSILON)
		*dist = f;
}

/*
 * FindFloatPlane
 */
int FindFloatPlane(vec3_t normal, vec_t dist) {
	int i;
	const map_plane_t *p;
	int hash;

	SnapPlane(normal, &dist);
	hash = (int) fabs(dist) / 8;
	hash &= (PLANE_HASHES - 1);

	// search the border bins as well
	for (i = -1; i <= 1; i++) {
		const int h = (hash + i) & (PLANE_HASHES - 1);
		for (p = plane_hash[h]; p; p = p->hash_chain) {
			if (PlaneEqual(p, normal, dist))
				return p - map_planes;
		}
	}

	return CreateNewFloatPlane(normal, dist);
}

/*
 * PlaneFromPoints
 */
static int PlaneFromPoints(const vec3_t p0, const vec3_t p1, const vec3_t p2) {
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
static int BrushContents(const map_brush_t * b) {
	int contents;
	const side_t *s;
	int i;
	int trans;

	s = &b->original_sides[0];
	contents = s->contents;
	trans = d_bsp.texinfo[s->texinfo].flags;
	for (i = 1; i < b->num_sides; i++, s++) {
		trans |= d_bsp.texinfo[s->texinfo].flags;
		if (s->contents != contents) {
			Com_Verbose("Entity %i, Brush %i: mixed face contents\n",
					b->entity_num, b->brush_num);
			break;
		}
	}

	// if any side is translucent, mark the contents and change solid to window
	if (trans & (SURF_ALPHA_TEST | SURF_BLEND_33 | SURF_BLEND_66)) {
		contents |= CONTENTS_TRANSLUCENT;
		if (contents & CONTENTS_SOLID) {
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
static void AddBrushBevels(map_brush_t * b) {
	int axis, dir;
	int i, j, k, l, order;
	side_t sidetemp;
	map_brush_texture_t tdtemp;
	side_t *s, *s2;
	vec3_t normal;
	float dist;
	winding_t *w, *w2;
	vec3_t vec, vec2;
	float d;

	// add the axial planes
	order = 0;
	for (axis = 0; axis < 3; axis++) {
		for (dir = -1; dir <= 1; dir += 2, order++) {
			// see if the plane is already present
			for (i = 0, s = b->original_sides; i < b->num_sides; i++, s++) {
				if (map_planes[s->plane_num].normal[axis] == dir)
					break;
			}

			if (i == b->num_sides) { // add a new side
				if (num_map_brush_sides == MAX_BSP_BRUSH_SIDES)
					Com_Error(ERR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
				num_map_brush_sides++;
				b->num_sides++;
				VectorClear(normal);
				normal[axis] = dir;
				if (dir == 1)
					dist = b->maxs[axis];
				else
					dist = -b->mins[axis];
				s->plane_num = FindFloatPlane(normal, dist);
				s->texinfo = b->original_sides[0].texinfo;
				s->contents = b->original_sides[0].contents;
				s->bevel = true;
				c_box_bevels++;
			}
			// if the plane is not in it canonical order, swap it
			if (i != order) {
				sidetemp = b->original_sides[order];
				b->original_sides[order] = b->original_sides[i];
				b->original_sides[i] = sidetemp;

				j = b->original_sides - map_brush_sides;
				tdtemp = map_brush_textures[j + order];
				map_brush_textures[j + order] = map_brush_textures[j + i];
				map_brush_textures[j + i] = tdtemp;
			}
		}
	}

	// add the edge bevels
	if (b->num_sides == 6)
		return; // pure axial

	// test the non-axial plane edges
	for (i = 6; i < b->num_sides; i++) {
		s = b->original_sides + i;
		w = s->winding;
		if (!w)
			continue;
		for (j = 0; j < w->numpoints; j++) {
			k = (j + 1) % w->numpoints;
			VectorSubtract(w->p[j], w->p[k], vec);
			if (VectorNormalize(vec) < 0.5)
				continue;
			SnapVector(vec);
			for (k = 0; k < 3; k++)
				if (vec[k] == -1 || vec[k] == 1)
					break; // axial
			if (k != 3)
				continue; // only test non-axial edges

			// try the six possible slanted axials from this edge
			for (axis = 0; axis < 3; axis++) {
				for (dir = -1; dir <= 1; dir += 2) {
					// construct a plane
					VectorClear(vec2);
					vec2[axis] = dir;
					CrossProduct(vec, vec2, normal);
					if (VectorNormalize(normal) < 0.5)
						continue;
					dist = DotProduct(w->p[j], normal);

					// if all the points on all the sides are
					// behind this plane, it is a proper edge bevel
					for (k = 0; k < b->num_sides; k++) {
						float minBack;

						// if this plane has already been used, skip it
						if (PlaneEqual(
								&map_planes[b->original_sides[k].plane_num],
								normal, dist))
							break;

						w2 = b->original_sides[k].winding;
						if (!w2)
							continue;
						minBack = 0.0f;
						for (l = 0; l < w2->numpoints; l++) {
							d = DotProduct(w2->p[l], normal) - dist;
							if (d > 0.1)
								break; // point in front
							if (d < minBack)
								minBack = d;
						}
						// if some point was at the front
						if (l != w2->numpoints)
							break;
						// if no points at the back then the winding is on the
						// bevel plane
						if (minBack > -0.1f)
							break;
					}

					if (k != b->num_sides)
						continue; // wasn't part of the outer hull
					// add this plane
					if (num_map_brush_sides == MAX_BSP_BRUSH_SIDES)
						Com_Error(ERR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
					num_map_brush_sides++;
					s2 = &b->original_sides[b->num_sides];
					s2->plane_num = FindFloatPlane(normal, dist);
					s2->texinfo = b->original_sides[0].texinfo;
					s2->contents = b->original_sides[0].contents;
					s2->bevel = true;
					c_edge_bevels++;
					b->num_sides++;
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
static boolean_t MakeBrushWindings(map_brush_t * ob) {
	int i, j;
	side_t *side;

	ClearBounds(ob->mins, ob->maxs);

	for (i = 0; i < ob->num_sides; i++) {
		const map_plane_t *plane = &map_planes[ob->original_sides[i].plane_num];
		winding_t *w = BaseWindingForPlane(plane->normal, plane->dist);
		for (j = 0; j < ob->num_sides && w; j++) {
			if (i == j)
				continue;
			// back side clipaway
			if (ob->original_sides[j].plane_num
					== (ob->original_sides[j].plane_num ^ 1))
				continue;
			if (ob->original_sides[j].bevel)
				continue;
			plane = &map_planes[ob->original_sides[j].plane_num ^ 1];
			ChopWindingInPlace(&w, plane->normal, plane->dist, 0); //CLIP_EPSILON);
		}

		side = &ob->original_sides[i];
		side->winding = w;
		if (w) {
			side->visible = true;
			for (j = 0; j < w->numpoints; j++)
				AddPointToBounds(w->p[j], ob->mins, ob->maxs);
		}
	}

	for (i = 0; i < 3; i++) {
		if (ob->mins[0] < -MAX_WORLD_WIDTH || ob->maxs[0] > MAX_WORLD_WIDTH)
			Com_Verbose("entity %i, brush %i: bounds out of range\n",
					ob->entity_num, ob->brush_num);
		if (ob->mins[0] > MAX_WORLD_WIDTH || ob->maxs[0] < -MAX_WORLD_WIDTH)
			Com_Verbose("entity %i, brush %i: no visible sides on brush\n",
					ob->entity_num, ob->brush_num);
	}

	return true;
}

/*
 * SetImpliedFlags
 */
static void SetImpliedFlags(side_t *side, const char *tex) {

	if (!strcmp(tex, "common/caulk"))
		side->surf |= SURF_NO_DRAW;
	else if (!strcmp(tex, "common/trigger"))
		side->surf |= SURF_NO_DRAW;
	else if (!strcmp(tex, "common/sky"))
		side->surf |= SURF_SKY;
	else if (!strcmp(tex, "common/hint"))
		side->surf |= SURF_HINT;
	else if (!strcmp(tex, "common/clip"))
		side->contents |= CONTENTS_PLAYER_CLIP;
	else if (!strcmp(tex, "common/ladder"))
		side->contents |= CONTENTS_LADDER;

	if (strstr(tex, "lava")) {
		side->surf |= SURF_WARP;
		side->contents |= CONTENTS_LAVA;
	}

	if (strstr(tex, "slime")) {
		side->surf |= SURF_WARP;
		side->contents |= CONTENTS_SLIME;
	}

	if (strstr(tex, "water")) {
		side->surf |= SURF_WARP;
		side->contents |= CONTENTS_WATER;
	}
}

/*
 * ParseBrush
 */
static void ParseBrush(entity_t *mapent) {
	map_brush_t *b;
	int i, j, k;
	side_t *side, *s2;
	int plane_num;
	map_brush_texture_t td;
	vec3_t planepts[3];

	if (num_map_brushes == MAX_BSP_BRUSHES)
		Com_Error(ERR_FATAL, "num_map_brushes == MAX_BSP_BRUSHES\n");

	b = &map_brushes[num_map_brushes];
	b->original_sides = &map_brush_sides[num_map_brush_sides];
	b->entity_num = num_entities - 1;
	b->brush_num = num_map_brushes - mapent->first_brush;

	do {
		if (!GetToken(true))
			break;
		if (!strcmp(token, "}"))
			break;

		if (num_map_brush_sides == MAX_BSP_BRUSH_SIDES)
			Com_Error(ERR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
		side = &map_brush_sides[num_map_brush_sides];

		// read the three point plane definition
		for (i = 0; i < 3; i++) {
			if (i != 0)
				GetToken(true);
			if (strcmp(token, "("))
				Com_Error(ERR_FATAL, "Parsing brush\n");

			for (j = 0; j < 3; j++) {
				GetToken(false);
				planepts[i][j] = atof(token);
			}

			GetToken(false);
			if (strcmp(token, ")"))
				Com_Error(ERR_FATAL, "Parsing brush\n");
		}

		memset(&td, 0, sizeof(td));

		// read the texturedef
		GetToken(false);

		if (strlen(token) > sizeof(td.name) - 1)
			Com_Error(ERR_FATAL, "Texture name \"%s\" is too long.\n", token);

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

		if (TokenAvailable()) {
			GetToken(false);
			side->contents = atoi(token);
			GetToken(false);
			side->surf = td.flags = atoi(token);
			GetToken(false);
			td.value = atoi(token);
		} else {
			side->contents = CONTENTS_SOLID;
			side->surf = td.flags = 0;
			td.value = 0;
		}

		// resolve implicit surface and contents flags
		SetImpliedFlags(side, td.name);

		// translucent objects are automatically classified as detail
		if (side->surf & (SURF_ALPHA_TEST | SURF_BLEND_33 | SURF_BLEND_66))
			side->contents |= CONTENTS_DETAIL;
		if (side->contents & (CONTENTS_PLAYER_CLIP | CONTENTS_MONSTER_CLIP))
			side->contents |= CONTENTS_DETAIL;
		if (fulldetail)
			side->contents &= ~CONTENTS_DETAIL;
		if (!(side->contents & ((LAST_VISIBLE_CONTENTS - 1)
				| CONTENTS_PLAYER_CLIP | CONTENTS_MONSTER_CLIP | CONTENTS_MIST)))
			side->contents |= CONTENTS_SOLID;

		// hints and skips are never detail, and have no content
		if (side->surf & (SURF_HINT | SURF_SKIP)) {
			side->contents = 0;
			side->surf &= ~CONTENTS_DETAIL;
		}

		// find the plane number
		plane_num = PlaneFromPoints(planepts[0], planepts[1], planepts[2]);
		if (plane_num == -1) {
			Com_Verbose("Entity %i, Brush %i: plane with no normal\n",
					b->entity_num, b->brush_num);
			continue;
		}

		// see if the plane has been used already
		for (k = 0; k < b->num_sides; k++) {
			s2 = b->original_sides + k;
			if (s2->plane_num == plane_num) {
				Com_Verbose("Entity %i, Brush %i: duplicate plane\n",
						b->entity_num, b->brush_num);
				break;
			}
			if (s2->plane_num == (plane_num ^ 1)) {
				Com_Verbose("Entity %i, Brush %i: mirrored plane\n",
						b->entity_num, b->brush_num);
				break;
			}
		}
		if (k != b->num_sides)
			continue; // duplicated

		// keep this side
		side = b->original_sides + b->num_sides;
		side->plane_num = plane_num;
		side->texinfo = TexinfoForBrushTexture(&map_planes[plane_num], &td,
				vec3_origin);

		// save the td off in case there is an origin brush and we
		// have to recalculate the texinfo
		map_brush_textures[num_map_brush_sides] = td;

		num_map_brush_sides++;
		b->num_sides++;
	} while (true);

	// get the content for the entire brush
	b->contents = BrushContents(b);

	// allow detail brushes to be removed
	if (nodetail && (b->contents & CONTENTS_DETAIL)) {
		b->num_sides = 0;
		return;
	}

	// allow water brushes to be removed
	if (nowater && (b->contents & (CONTENTS_LAVA | CONTENTS_SLIME
			| CONTENTS_WATER))) {
		b->num_sides = 0;
		return;
	}

	// create windings for sides and bounds for brush
	MakeBrushWindings(b);

	// brushes that will not be visible at all will never be
	// used as bsp splitters
	if (b->contents & (CONTENTS_PLAYER_CLIP | CONTENTS_MONSTER_CLIP)) {
		c_clip_brushes++;
		for (i = 0; i < b->num_sides; i++)
			b->original_sides[i].texinfo = TEXINFO_NODE;
	}

	// origin brushes are removed, but they set
	// the rotation origin for the rest of the brushes
	// in the entity.  After the entire entity is parsed,
	// the plane_nums and texinfos will be adjusted for
	// the origin brush
	if (b->contents & CONTENTS_ORIGIN) {
		char string[32];
		vec3_t origin;

		if (num_entities == 1) {
			Com_Error(
					ERR_FATAL,
					"Entity %i, Brush %i: origin brushes not allowed in world\n",
					b->entity_num, b->brush_num);
			return;
		}

		VectorAdd(b->mins, b->maxs, origin);
		VectorScale(origin, 0.5, origin);

		sprintf(string, "%i %i %i", (int)origin[0], (int)origin[1],
				(int)origin[2]);
		SetKeyValue(&entities[b->entity_num], "origin", string);

		VectorCopy(origin, entities[b->entity_num].origin);

		// don't keep this brush
		b->num_sides = 0;

		return;
	}

	AddBrushBevels(b);

	num_map_brushes++;
	mapent->num_brushes++;
}

/*
 * MoveBrushesToWorld
 *
 * Takes all of the brushes from the current entity and adds them to the
 * world's brush list.
 *
 * Used by func_group and func_areaportal
 */
static void MoveBrushesToWorld(entity_t *ent) {
	int new_brushes;
	int world_brushes;
	map_brush_t *temp;
	int i;

	// this is pretty gross, because the brushes are expected to be
	// in linear order for each entity

	new_brushes = ent->num_brushes;
	world_brushes = entities[0].num_brushes;

	temp = Z_Malloc(new_brushes * sizeof(map_brush_t));
	memcpy(temp, map_brushes + ent->first_brush,
			new_brushes * sizeof(map_brush_t));

	// make space to move the brushes (overlapped copy)
	memmove(map_brushes + world_brushes + new_brushes,
			map_brushes + world_brushes,
			sizeof(map_brush_t) * (num_map_brushes - world_brushes - new_brushes));

	// copy the new brushes down
	memcpy(map_brushes + world_brushes, temp, sizeof(map_brush_t) * new_brushes);

	// fix up indexes
	entities[0].num_brushes += new_brushes;
	for (i = 1; i < num_entities; i++)
		entities[i].first_brush += new_brushes;
	Z_Free(temp);

	ent->num_brushes = 0;
}

/*
 * ParseMapEntity
 */
static boolean_t ParseMapEntity(void) {
	entity_t *mapent;
	epair_t *e;
	side_t *s;
	int i, j;
	vec_t newdist;
	map_brush_t *b;

	if (!GetToken(true))
		return false;

	if (strcmp(token, "{"))
		Com_Error(ERR_FATAL, "ParseMapEntity: { not found\n");

	if (num_entities == MAX_BSP_ENTITIES)
		Com_Error(ERR_FATAL, "num_entities == MAX_BSP_ENTITIES\n");

	mapent = &entities[num_entities];
	num_entities++;
	memset(mapent, 0, sizeof(*mapent));
	mapent->first_brush = num_map_brushes;
	mapent->num_brushes = 0;

	do {
		if (!GetToken(true))
			Com_Error(ERR_FATAL, "ParseMapEntity: EOF without closing brace\n");
		if (!strcmp(token, "}"))
			break;
		if (!strcmp(token, "{"))
			ParseBrush(mapent);
		else {
			e = ParseEpair();
			e->next = mapent->epairs;
			mapent->epairs = e;
		}
	} while (true);

	GetVectorForKey(mapent, "origin", mapent->origin);

	// if there was an origin brush, offset all of the planes and texinfo
	if (mapent->origin[0] || mapent->origin[1] || mapent->origin[2]) {
		for (i = 0; i < mapent->num_brushes; i++) {
			b = &map_brushes[mapent->first_brush + i];
			for (j = 0; j < b->num_sides; j++) {

				s = &b->original_sides[j];

				newdist
						= map_planes[s->plane_num].dist
								- DotProduct(map_planes[s->plane_num].normal, mapent->origin);

				s->plane_num = FindFloatPlane(map_planes[s->plane_num].normal,
						newdist);

				s->texinfo = TexinfoForBrushTexture(&map_planes[s->plane_num],
						&map_brush_textures[s - map_brush_sides],
						mapent->origin);
			}
			MakeBrushWindings(b);
		}
	}

	// group entities are just for editor convenience
	// toss all brushes into the world entity
	if (!strcmp("func_group", ValueForKey(mapent, "classname"))) {
		MoveBrushesToWorld(mapent);
		mapent->num_brushes = 0;
		return true;
	}

	// areaportal entities move their brushes, but don't eliminate the entity
	if (!strcmp("func_areaportal", ValueForKey(mapent, "classname"))) {
		char str[128];

		if (mapent->num_brushes != 1)
			Com_Error(
					ERR_FATAL,
					"ParseMapEntity: %i func_areaportal can only be a single brush\n",
					num_entities - 1);

		b = &map_brushes[num_map_brushes - 1];
		b->contents = CONTENTS_AREA_PORTAL;
		c_area_portals++;
		mapent->area_portal_num = c_area_portals;
		// set the portal number as "style"
		sprintf(str, "%i", c_area_portals);
		SetKeyValue(mapent, "areaportal", str);
		MoveBrushesToWorld(mapent);
		return true;
	}

	return true;
}

/*
 * LoadMapFile
 */
void LoadMapFile(const char *file_name) {
	int subdivide;
	int i;

	Com_Verbose("--- LoadMapFile ---\n");

	LoadScriptFile(file_name);

	memset(map_brushes, 0, sizeof(map_brush_t) * MAX_BSP_BRUSHES);
	num_map_brushes = 0;

	memset(map_brush_sides, 0, sizeof(side_t) * MAX_BSP_SIDES);
	num_map_brush_sides = 0;

	memset(map_brush_textures, 0, sizeof(map_brush_texture_t) * MAX_BSP_SIDES);

	memset(map_planes, 0, sizeof(map_plane_t) * MAX_BSP_PLANES);
	num_map_planes = 0;

	num_entities = 0;
	while (ParseMapEntity()) {
	}

	subdivide = atoi(ValueForKey(&entities[0], "subdivide"));

	if (subdivide >= 256 && subdivide <= 2048) {
		Com_Verbose("Using subdivide %d from worldspawn.\n", subdivide);
		subdivide_size = subdivide;
	}

	ClearBounds(map_mins, map_maxs);
	for (i = 0; i < entities[0].num_brushes; i++) {
		if (map_brushes[i].mins[0] > MAX_WORLD_WIDTH)
			continue; // no valid points
		AddPointToBounds(map_brushes[i].mins, map_mins, map_maxs);
		AddPointToBounds(map_brushes[i].maxs, map_mins, map_maxs);
	}

	Com_Verbose("%5i brushes\n", num_map_brushes);
	Com_Verbose("%5i clip brushes\n", c_clip_brushes);
	Com_Verbose("%5i total sides\n", num_map_brush_sides);
	Com_Verbose("%5i box bevels\n", c_box_bevels);
	Com_Verbose("%5i edge bevels\n", c_edge_bevels);
	Com_Verbose("%5i entities\n", num_entities);
	Com_Verbose("%5i planes\n", num_map_planes);
	Com_Verbose("%5i area portals\n", c_area_portals);
	Com_Verbose("size: %5.0f,%5.0f,%5.0f to %5.0f,%5.0f,%5.0f\n", map_mins[0],
			map_mins[1], map_mins[2], map_maxs[0], map_maxs[1], map_maxs[2]);
}
