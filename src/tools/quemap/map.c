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

#include "brush.h"
#include "bsp.h"
#include "map.h"
#include "material.h"
#include "parse.h"
#include "qbsp.h"
#include "texinfo.h"

uint16_t num_entities;
entity_t entities[MAX_BSP_ENTITIES];

int32_t num_brushes;
brush_t brushes[MAX_BSP_BRUSHES];

#define MAX_BSP_SIDES (MAX_BSP_BRUSHES * 6)

static int32_t num_brush_sides;
static brush_side_t brush_sides[MAX_BSP_SIDES];
static brush_texture_t brush_textures[MAX_BSP_SIDES];

int32_t num_planes;
plane_t planes[MAX_BSP_PLANES];

#define	PLANE_HASHES 4096
static plane_t *plane_hash[PLANE_HASHES];

vec3_t map_mins, map_maxs;

static int32_t c_box_bevels;
static int32_t c_edge_bevels;
static int32_t c_area_portals;
static int32_t c_clip_brushes;

/**
 * @brief Set the type of the plane according to it's (snapped) normal vector.
 */
static int32_t PlaneTypeForNormal(const vec3_t normal) {

	if (normal[0] == 1.0 || normal[0] == -1.0) {
		return PLANE_X;
	}
	if (normal[1] == 1.0 || normal[1] == -1.0) {
		return PLANE_Y;
	}
	if (normal[2] == 1.0 || normal[2] == -1.0) {
		return PLANE_Z;
	}

	const vec_t ax = fabs(normal[0]);
	const vec_t ay = fabs(normal[1]);
	const vec_t az = fabs(normal[2]);

	if (ax >= ay && ax >= az) {
		return PLANE_ANY_X;
	}
	if (ay >= ax && ay >= az) {
		return PLANE_ANY_Y;
	}
	return PLANE_ANY_Z;
}

#define	NORMAL_EPSILON	0.00001
#define	DIST_EPSILON	0.005

/**
 * @brief
 */
static _Bool PlaneEqual(const plane_t *p, const vec3_t normal, const dvec_t dist) {

	const vec_t ne = NORMAL_EPSILON;
	const dvec_t de = DIST_EPSILON;

	if ((p->dist == dist || fabsl(p->dist - dist) < de) &&
		(p->normal[0] == normal[0] || fabs(p->normal[0] - normal[0]) < ne) &&
		(p->normal[1] == normal[1] || fabs(p->normal[1] - normal[1]) < ne) &&
		(p->normal[2] == normal[2] || fabs(p->normal[2] - normal[2]) < ne)) {
		return true;
	}

	return false;
}

/**
 * @brief
 */
static inline void AddPlaneToHash(plane_t *p) {

	const uint16_t hash = ((uint32_t) fabs(p->dist)) & (PLANE_HASHES - 1);

	p->hash_chain = plane_hash[hash];
	plane_hash[hash] = p;
}

/**
 * @brief
 */
static int32_t CreateNewFloatPlane(vec3_t normal, vec_t dist) {
	plane_t *p;

	// bad plane
	if (VectorLength(normal) < 0.5) {
		return -1;
	}

	// create a new plane
	if (num_planes + 2 > MAX_BSP_PLANES) {
		Com_Error(ERROR_FATAL, "MAX_BSP_PLANES\n");
	}

	p = &planes[num_planes];
	VectorCopy(normal, p->normal);
	p->dist = dist;

	p->type = (p + 1)->type = PlaneTypeForNormal(p->normal);
	VectorNegate(normal, (p + 1)->normal);
	(p + 1)->dist = -dist;

	num_planes += 2;

	// always put axial planes facing positive first
	if (AXIAL(p)) {
		if (p->normal[0] < 0.0 || p->normal[1] < 0.0 || p->normal[2] < 0.0) {
			// flip order
			plane_t temp = *p;
			*p = *(p + 1);
			*(p + 1) = temp;

			AddPlaneToHash(p);
			AddPlaneToHash(p + 1);
			return num_planes - 1;
		}
	}

	AddPlaneToHash(p);
	AddPlaneToHash(p + 1);
	return num_planes - 2;
}

/**
 * @brief If the specified normal is very close to an axis, align with it.
 */
static void SnapNormal(vec3_t normal) {

	_Bool snap = false;

	for (int32_t i = 0; i < 3; i++) {
		if (normal[i] != 0.0) {
			if (normal[i] > -NORMAL_EPSILON && normal[i] < NORMAL_EPSILON) {
				normal[i] = 0.0;
				snap = true;
			}
		}
	}

	if (snap) {
		VectorNormalize(normal);
	}
}

/**
 * @brief
 */
static void SnapPlane(vec3_t normal, dvec_t *dist) {

	SnapNormal(normal);

	// snap axial planes to integer distances
	if (PlaneTypeForNormal(normal) <= PLANE_Z) {

		const vec_t f = floor(*dist + 0.5);
		if (fabs(*dist - f) < DIST_EPSILON) {
			*dist = f;
		}
	}
}

/**
 * @brief
 */
int32_t FindPlane(vec3_t normal, dvec_t dist) {

	SnapPlane(normal, &dist);
	const uint16_t hash = ((uint32_t) fabsl(dist)) & (PLANE_HASHES - 1);

	// search the border bins as well
	for (int32_t i = -1; i <= 1; i++) {
		const uint16_t h = (hash + i) & (PLANE_HASHES - 1);
		const plane_t *p = plane_hash[h];

		while (p) {
			if (PlaneEqual(p, normal, dist)) {
				return (int32_t) (ptrdiff_t) (p - planes);
			}
			p = p->hash_chain;
		}
	}

	return CreateNewFloatPlane(normal, dist);
}

/**
 * @brief
 */
static int32_t PlaneFromPoints(const vec3_t p0, const vec3_t p1, const vec3_t p2) {
	vec3_t t1, t2, normal;

	VectorSubtract(p0, p1, t1);
	VectorSubtract(p2, p1, t2);
	CrossProduct(t1, t2, normal);
	VectorNormalize(normal);

	const dvec_t dist = DotProduct(p0, normal);

	return FindPlane(normal, dist);
}

/**
 * @brief
 */
static int32_t BrushContents(const brush_t *b) {
	int32_t contents;
	const brush_side_t *s;
	int32_t i;
	int32_t trans;

	s = &b->original_sides[0];
	contents = s->contents;
	trans = bsp_file.texinfo[s->texinfo].flags;
	for (i = 1; i < b->num_sides; i++, s++) {
		trans |= bsp_file.texinfo[s->texinfo].flags;
		if (s->contents != contents) {
			Com_Verbose("Entity %i, Brush %i: mixed face contents\n", b->entity_num, b->brush_num);
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

/**
 * @brief Adds any additional planes necessary to allow the brush to be expanded
 * against axial bounding boxes
 */
static void AddBrushBevels(brush_t *b) {
	int32_t axis, dir;
	int32_t i, j, k, l, order;
	brush_side_t sidetemp;
	brush_texture_t tdtemp;
	brush_side_t *s, *s2;
	vec3_t normal;
	vec_t dist;
	winding_t *w, *w2;
	vec3_t vec, vec2;
	vec_t d;

	// add the axial planes
	order = 0;
	for (axis = 0; axis < 3; axis++) {
		for (dir = -1; dir <= 1; dir += 2, order++) {
			// see if the plane is already present
			for (i = 0, s = b->original_sides; i < b->num_sides; i++, s++) {
				if (planes[s->plane_num].normal[axis] == dir) {
					break;
				}
			}

			if (i == b->num_sides) { // add a new side
				if (num_brush_sides == MAX_BSP_BRUSH_SIDES) {
					Com_Error(ERROR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
				}
				num_brush_sides++;
				b->num_sides++;
				VectorClear(normal);
				normal[axis] = dir;
				if (dir == 1) {
					dist = b->maxs[axis];
				} else {
					dist = -b->mins[axis];
				}
				s->plane_num = FindPlane(normal, dist);
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

				j = (int32_t) (ptrdiff_t) (b->original_sides - brush_sides);
				tdtemp = brush_textures[j + order];
				brush_textures[j + order] = brush_textures[j + i];
				brush_textures[j + i] = tdtemp;
			}
		}
	}

	// add the edge bevels
	if (b->num_sides == 6) {
		return;    // pure axial
	}

	// test the non-axial plane edges
	for (i = 6; i < b->num_sides; i++) {
		s = b->original_sides + i;
		w = s->winding;
		if (!w) {
			continue;
		}
		for (j = 0; j < w->num_points; j++) {
			k = (j + 1) % w->num_points;
			VectorSubtract(w->points[j], w->points[k], vec);
			if (VectorNormalize(vec) < 0.5) {
				continue;
			}
			SnapNormal(vec);
			for (k = 0; k < 3; k++) {
				if (vec[k] == -1.0 || vec[k] == 1.0 || (vec[k] == 0.0 && vec[(k + 1) % 3] == 0.0)) {
					break; // axial
				}
			}
			if (k != 3) {
				continue; // only test non-axial edges
			}

			// try the six possible slanted axials from this edge
			for (axis = 0; axis < 3; axis++) {
				for (dir = -1; dir <= 1; dir += 2) {
					// construct a plane
					VectorClear(vec2);
					vec2[axis] = dir;
					CrossProduct(vec, vec2, normal);
					if (VectorNormalize(normal) < 0.5) {
						continue;
					}
					dist = DotProduct(w->points[j], normal);

					// if all the points on all the sides are
					// behind this plane, it is a proper edge bevel
					for (k = 0; k < b->num_sides; k++) {
						vec_t minBack;

						// if this plane has already been used, skip it
						if (PlaneEqual(&planes[b->original_sides[k].plane_num], normal, dist)) {
							break;
						}

						w2 = b->original_sides[k].winding;
						if (!w2) {
							continue;
						}
						minBack = 0.0f;
						for (l = 0; l < w2->num_points; l++) {
							d = DotProduct(w2->points[l], normal) - dist;
							if (d > 0.1) {
								break;    // point in front
							}
							if (d < minBack) {
								minBack = d;
							}
						}
						// if some point was at the front
						if (l != w2->num_points) {
							break;
						}
						// if no points at the back then the winding is on the
						// bevel plane
						if (minBack > -0.1f) {
							break;
						}
					}

					if (k != b->num_sides) {
						continue;    // wasn't part of the outer hull
					}
					// add this plane
					if (num_brush_sides == MAX_BSP_BRUSH_SIDES) {
						Com_Error(ERROR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
					}

					s2 = &b->original_sides[b->num_sides++];
					s2->plane_num = FindPlane(normal, dist);
					s2->texinfo = b->original_sides[0].texinfo;
					s2->contents = b->original_sides[0].contents;
					s2->bevel = true;

					num_brush_sides++;
					c_edge_bevels++;
				}
			}
		}
	}
}

/**
 * @brief Makes basewindigs for sides and mins / maxs for the brush
 */
static _Bool MakeBrushWindings(brush_t *ob) {

	ClearBounds(ob->mins, ob->maxs);

	for (int32_t i = 0; i < ob->num_sides; i++) {
		const plane_t *plane = &planes[ob->original_sides[i].plane_num];
		winding_t *w = WindingForPlane(plane->normal, plane->dist);
		for (int32_t j = 0; j < ob->num_sides && w; j++) {
			if (i == j) {
				continue;
			}
			// back side clipaway
			if (ob->original_sides[j].plane_num == (ob->original_sides[j].plane_num ^ 1)) {
				continue;
			}
			if (ob->original_sides[j].bevel) {
				continue;
			}
			plane = &planes[ob->original_sides[j].plane_num ^ 1];
			ChopWindingInPlace(&w, plane->normal, plane->dist, 0); //CLIP_EPSILON);
		}

		brush_side_t *side = &ob->original_sides[i];
		side->winding = w;
		if (w) {
			side->visible = true;
			for (int32_t j = 0; j < w->num_points; j++) {
				AddPointToBounds(w->points[j], ob->mins, ob->maxs);
			}
		}
	}

	for (int32_t i = 0; i < 3; i++) {
		if (ob->mins[0] < MIN_WORLD_COORD || ob->maxs[0] > MAX_WORLD_COORD) {
			Com_Verbose("entity %i, brush %i: bounds out of range\n", ob->entity_num, ob->brush_num);
		}
		if (ob->mins[0] > MAX_WORLD_COORD || ob->maxs[0] < MIN_WORLD_COORD)
			Com_Verbose("entity %i, brush %i: no visible sides on brush\n", ob->entity_num,
			            ob->brush_num);
	}

	return true;
}

/**
 * @brief
 */
static void SetMaterialFlags(brush_side_t *side, brush_texture_t *td) {

	const cm_material_t *material = LoadMaterial(td->name, ASSET_CONTEXT_TEXTURES);
	if (material) {
		if (material->contents) {
			if (side->contents == 0) {
				side->contents = material->contents;
			}
		}
		if (material->surface) {
			if (td->flags == 0) {
				td->flags = material->surface;
			}
		}
		if (material->light) {
			if (td->value == 0.0) {
				td->value = material->light;
			}
		}
	}

	if (!g_strcmp0(td->name, "common/areaportal")) {
		side->contents |= CONTENTS_AREA_PORTAL;
		td->flags |= SURF_NO_DRAW;
	} else if (!g_strcmp0(td->name, "common/monsterclip") || !g_strcmp0(td->name, "common/botclip")) {
		side->contents |= CONTENTS_MONSTER_CLIP;
	} else if (!g_strcmp0(td->name, "common/caulk") || !g_strcmp0(td->name, "common/nodraw")) {
		td->flags |= SURF_NO_DRAW;
	} else if (!g_strcmp0(td->name, "common/clip")) {
		side->contents |= CONTENTS_PLAYER_CLIP;
	} else if (!g_strcmp0(td->name, "common/hint")) {
		td->flags |= SURF_HINT;
	} else if (!g_strcmp0(td->name, "common/ladder")) {
		side->contents |= CONTENTS_LADDER | CONTENTS_DETAIL | CONTENTS_WINDOW;
		td->flags |= SURF_NO_DRAW;
	} else if (!g_strcmp0(td->name, "common/origin")) {
		side->contents |= CONTENTS_ORIGIN;
	} else if (!g_strcmp0(td->name, "common/skip")) {
		td->flags |= SURF_SKIP;
	} else if (!g_strcmp0(td->name, "common/sky")) {
		td->flags |= SURF_SKY;
	} else if (!g_strcmp0(td->name, "common/trigger")) {
		side->contents |= CONTENTS_DETAIL | CONTENTS_WINDOW;
		td->flags |= SURF_NO_DRAW;
	}
}

/**
 * @brief
 */
static brush_t *ParseBrush(parser_t *parser, entity_t *entity) {
	char token[MAX_TOKEN_CHARS];

	brush_t *brush = NULL;

	if (Parse_IsEOF(parser)) {
		return NULL;
	}

	Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));

	if (!g_strcmp0(token, "{")) {

		if (num_brushes == MAX_BSP_BRUSHES) {
			Com_Error(ERROR_FATAL, "MAX_BSP_BRUSHES\n");
		}

		brush = &brushes[num_brushes];
		num_brushes++;

		memset(brush, 0, sizeof(*brush));

		brush->original_sides = &brush_sides[num_brush_sides];
		brush->entity_num = num_entities - 1;
		brush->brush_num = num_brushes - entity->first_brush;

		while (true) {

			if (!Parse_Token(parser, PARSE_DEFAULT | PARSE_PEEK, token, sizeof(token))) {
				Com_Error(ERROR_FATAL, "EOF without closing brush\n");
			}

			if (!g_strcmp0(token, "}")) {
				Parse_SkipToken(parser, PARSE_DEFAULT);
				break;
			}

			if (num_brush_sides == MAX_BSP_BRUSH_SIDES) {
				Com_Error(ERROR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
			}

			brush_side_t *side = &brush_sides[num_brush_sides];

			vec3_t points[3];

			// read the three point plane definition
			for (int32_t i = 0; i < 3; i++) {

				Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
				if (g_strcmp0(token, "(")) {
					Com_Error(ERROR_FATAL, "Invalid brush %d (%s)\n", num_brushes, token);
				}

				for (int32_t j = 0; j < 3; j++) {
					Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &points[i][j], 1);
				}

				Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
				if (g_strcmp0(token, ")")) {
					Com_Error(ERROR_FATAL, "Invalid brush %d (%s)\n", num_brushes, token);
				}
			}

			brush_texture_t td;
			memset(&td, 0, sizeof(td));

			// read the texturedef
			Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));

			if (strlen(token) > sizeof(td.name) - 1) {
				Com_Error(ERROR_FATAL, "Texture name \"%s\" is too long.\n", token);
			}

			g_strlcpy(td.name, token, sizeof(td.name));

			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &td.shift[0], 1);
			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &td.shift[1], 1);
			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &td.rotate, 1);
			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &td.scale[0], 1);
			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &td.scale[1], 1);

			if (!Parse_IsEOL(parser)) {
				Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &side->contents, 1);
				Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &td.flags, 1);
				Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &td.value, 1);
			}

			// resolve material-based surface and contents flags
			SetMaterialFlags(side, &td);

			side->surf = td.flags;

			// translucent objects are automatically classified as detail
			if (side->surf & (SURF_ALPHA_TEST | SURF_BLEND_33 | SURF_BLEND_66)) {
				side->contents |= CONTENTS_DETAIL;
			}
			if (side->contents & (CONTENTS_PLAYER_CLIP | CONTENTS_MONSTER_CLIP)) {
				side->contents |= CONTENTS_DETAIL;
			}
			if (all_structural) {
				side->contents &= ~CONTENTS_DETAIL;
			}
			if (!(side->contents & ((LAST_VISIBLE_CONTENTS - 1) | CONTENTS_PLAYER_CLIP | CONTENTS_MONSTER_CLIP | CONTENTS_MIST))) {
				side->contents |= CONTENTS_SOLID;
			}

			// hints and skips are never detail, and have no content
			if (side->surf & (SURF_HINT | SURF_SKIP)) {
				side->contents = 0;
				side->surf &= ~CONTENTS_DETAIL;
			}

			// find the plane number
			int32_t plane_num = PlaneFromPoints(points[0], points[1], points[2]);
			if (plane_num == -1) {
				Mon_SendSelect(MON_WARN, brush->entity_num, brush->brush_num, "Bad plane");
				continue;
			}

			// see if the plane has been used already
			int32_t i;
			for (i = 0; i < brush->num_sides; i++) {
				brush_side_t *side = brush->original_sides + i;
				if (side->plane_num == plane_num) {
					Mon_SendSelect(MON_WARN, brush->entity_num, brush->brush_num, "Duplicate plane");
					break;
				}
				if (side->plane_num == (plane_num ^ 1)) {
					Mon_SendSelect(MON_WARN, brush->entity_num, brush->brush_num, "Mirrored plane");
					break;
				}
			}
			if (i != brush->num_sides) {
				continue; // duplicated
			}

			// keep this side
			side = brush->original_sides + brush->num_sides;
			side->plane_num = plane_num;
			side->texinfo = TexinfoForBrushTexture(&planes[plane_num], &td, vec3_origin);

			// save the td off in case there is an origin brush and we have to recalculate the texinfo
			brush_textures[num_brush_sides] = td;

			num_brush_sides++;
			brush->num_sides++;
		}

		// get the content for the entire brush
		brush->contents = BrushContents(brush);

		// allow detail brushes to be removed
		if (no_detail && (brush->contents & CONTENTS_DETAIL)) {
			num_brushes--;
			return NULL;
		}

		// allow water brushes to be removed
		if (no_water && (brush->contents & MASK_LIQUID)) {
			num_brushes--;
			return NULL;
		}

		// create windings for sides and bounds for brush
		MakeBrushWindings(brush);

		// brushes that will not be visible at all will never be used as bsp splitters
		if (brush->contents & (CONTENTS_PLAYER_CLIP | CONTENTS_MONSTER_CLIP)) {
			c_clip_brushes++;
			for (int32_t i = 0; i < brush->num_sides; i++) {
				brush->original_sides[i].texinfo = TEXINFO_NODE;
			}
		}

		// origin brushes are removed, but they set the rotation origin for the rest of the brushes
		// in the entity. After the entire entity is parsed, the plane_nums and texinfos will be adjusted for
		// the origin brush
		if (brush->contents & CONTENTS_ORIGIN) {

			if (num_entities == 1) {
				Mon_SendSelect(MON_ERROR, brush->entity_num, brush->brush_num, "Origin brush in world");
				Com_Error(ERROR_FATAL, "Entity %i, Brush %i: origin brushes not allowed in world\n",
						  brush->entity_num, brush->brush_num);
			}

			vec3_t origin;
			VectorAdd(brush->mins, brush->maxs, origin);
			VectorScale(origin, 0.5, origin);

			SetValueForKey(entity, "origin", va("%g %g %g", origin[0], origin[1], origin[2]));
			num_brushes--;
			return NULL;
		}

		AddBrushBevels(brush);
	}

	return brush;
}

/**
 * @brief Some entities are merged into the world, e.g. func_group and func_areaportal.
 */
static void MoveBrushesToWorld(entity_t *ent) {

	const int32_t new_brushes = ent->num_brushes;
	const int32_t world_brushes = entities[0].num_brushes;

	brush_t *temp = Mem_TagMalloc(new_brushes * sizeof(brush_t), MEM_TAG_BRUSH);
	memcpy(temp, brushes + ent->first_brush,
	       new_brushes * sizeof(brush_t));

	// make space to move the brushes (overlapped copy)
	memmove(brushes + world_brushes + new_brushes,
	        brushes + world_brushes,
	        sizeof(brush_t) * (num_brushes - world_brushes - new_brushes));

	// copy the new brushes down
	memcpy(brushes + world_brushes, temp, sizeof(brush_t) * new_brushes);

	// fix up indexes
	entities[0].num_brushes += new_brushes;
	for (int32_t i = 1; i < num_entities; i++) {
		entities[i].first_brush += new_brushes;
	}
	Mem_Free(temp);

	ent->num_brushes = 0;
}

/**
 * @brief
 */
static entity_t *ParseEntity(parser_t *parser) {
	char token[MAX_TOKEN_CHARS];

	entity_t *entity = NULL;

	if (Parse_IsEOF(parser)) {
		return NULL;
	}

	Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));

	if (!g_strcmp0(token, "{")) {

		if (num_entities == MAX_BSP_ENTITIES) {
			Com_Error(ERROR_FATAL, "MAX_BSP_ENTITIES\n");
		}

		entity = &entities[num_entities];
		num_entities++;

		entity->first_brush = num_brushes;

		while (true) {

			if (!Parse_Token(parser, PARSE_DEFAULT | PARSE_PEEK, token, sizeof(token))) {
				Com_Error(ERROR_FATAL, "EOF without closing entity\n");
			}

			if (!g_strcmp0(token, "}")) {
				Parse_SkipToken(parser, PARSE_DEFAULT);
				break;
			}

			if (!g_strcmp0(token, "{")) {
				brush_t *brush = ParseBrush(parser, entity);
				if (brush) {
					entity->num_brushes++;
				}
			} else {
				entity_key_value_t *e = Mem_TagMalloc(sizeof(*e), MEM_TAG_EPAIR);

				Parse_Token(parser, PARSE_DEFAULT, e->key, sizeof(e->key));
				Parse_Token(parser, PARSE_DEFAULT, e->value, sizeof(e->value));
				
				e->next = entity->values;
				entity->values = e;
			}
		}

		vec3_t origin;
		VectorForKey(entity, "origin", origin, NULL);

		// if there was an origin brush, offset all of the planes and texinfo
		if (!VectorCompare(origin, vec3_origin)) {
			for (int32_t i = 0; i < entity->num_brushes; i++) {
				brush_t *b = &brushes[entity->first_brush + i];
				for (int32_t j = 0; j < b->num_sides; j++) {

					brush_side_t *s = &b->original_sides[j];
					plane_t *p = &planes[s->plane_num];

					const vec_t dist = p->dist - DotProduct(p->normal, origin);

					s->plane_num = FindPlane(p->normal, dist);
					s->texinfo = TexinfoForBrushTexture(&planes[s->plane_num],
														&brush_textures[s - brush_sides],
														origin);
				}
				MakeBrushWindings(b);
			}
		}

		// group entities are just for editor convenience, move all brushes into the world
		const char *classname = ValueForKey(entity, "classname", NULL);
		if (!g_strcmp0("func_group", classname)) {
			MoveBrushesToWorld(entity);
			entity->num_brushes = 0;
		}

		// areaportal entities move their brushes into the world, but don't eliminate the entity
		if (!g_strcmp0("func_areaportal", classname)) {

			if (entity->num_brushes != 1) {
				Mon_SendSelect(MON_ERROR, num_entities - 1, 1, "func_areaportal must be a single brush");
				Com_Error(ERROR_FATAL, "%i func_areaportal must be a single brush\n", num_entities - 1);
			}

			brush_t *b = &brushes[num_brushes - 1];
			b->contents = CONTENTS_AREA_PORTAL;
			c_area_portals++;
			entity->area_portal_num = c_area_portals;

			SetValueForKey(entity, "areaportal", va("%d", entity->area_portal_num));
			MoveBrushesToWorld(entity);
		}
	}

	return entity;
}

/**
 * @brief
 */
void LoadMapFile(const char *filename) {

	Com_Verbose("--- LoadMapFile ---\n");

	memset(entities, 0, sizeof(entities));
	num_entities = 0;

	memset(brushes, 0, sizeof(brushes));
	num_brushes = 0;

	memset(brush_sides, 0, sizeof(brush_sides));
	num_brush_sides = 0;

	memset(brush_textures, 0, sizeof(brush_textures));

	memset(planes, 0, sizeof(planes));
	num_planes = 0;

	memset(plane_hash, 0, sizeof(plane_hash));

	void *buffer;
	if (Fs_Load(filename, &buffer) == -1) {
		Com_Error(ERROR_FATAL, "Failed to load %s\n", filename);
	}

	parser_t parser;
	Parse_Init(&parser, buffer, PARSER_DEFAULT);

	for (int32_t i = 0, models = 1; i < MAX_BSP_ENTITIES; i++) {

		entity_t *entity = ParseEntity(&parser);
		if (!entity) {
			break;
		}

		if (i > 0 && entity->num_brushes) {
			SetValueForKey(entity, "model", va("*%d", models++));
		}
	}

	ClearBounds(map_mins, map_maxs);

	for (int32_t i = 0; i < entities[0].num_brushes; i++) {
		if (brushes[i].mins[0] > MAX_WORLD_COORD) {
			continue; // no valid points
		}
		AddPointToBounds(brushes[i].mins, map_mins, map_maxs);
		AddPointToBounds(brushes[i].maxs, map_mins, map_maxs);
	}

	Com_Verbose("%5i brushes\n", num_brushes);
	Com_Verbose("%5i clip brushes\n", c_clip_brushes);
	Com_Verbose("%5i total sides\n", num_brush_sides);
	Com_Verbose("%5i box bevels\n", c_box_bevels);
	Com_Verbose("%5i edge bevels\n", c_edge_bevels);
	Com_Verbose("%5i entities\n", num_entities);
	Com_Verbose("%5i planes\n", num_planes);
	Com_Verbose("%5i area portals\n", c_area_portals);
	Com_Verbose("size: %5.0f,%5.0f,%5.0f to %5.0f,%5.0f,%5.0f\n",
				map_mins[0], map_mins[1], map_mins[2], map_maxs[0], map_maxs[1], map_maxs[2]);

	Fs_Free(buffer);
}
