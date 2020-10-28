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

int32_t num_entities;
entity_t entities[MAX_BSP_ENTITIES];

int32_t num_brushes;
brush_t brushes[MAX_BSP_BRUSHES];

static int32_t num_brush_sides;
static brush_side_t brush_sides[MAX_BSP_BRUSH_SIDES];
static brush_texture_t brush_textures[MAX_BSP_BRUSH_SIDES];

int32_t num_planes;
plane_t planes[MAX_BSP_PLANES];

#define	PLANE_HASHES 4096
static plane_t *plane_hash[PLANE_HASHES];

vec3_t map_mins, map_maxs;

static int32_t c_box_bevels;
static int32_t c_edge_bevels;
static int32_t c_area_portals;
static int32_t c_clip_brushes;

#define	NORMAL_EPSILON	0.00001
#define	DIST_EPSILON	0.005

/**
 * @brief
 */
static _Bool PlaneEqual(const plane_t *p, const vec3_t normal, double dist) {

	if ((EqualEpsilon(p->dist, dist, DIST_EPSILON)) &&
		Vec3_EqualEpsilon(p->normal, normal, NORMAL_EPSILON)) {
		return true;
	}

	return false;
}

/**
 * @brief
 */
static inline void AddPlaneToHash(plane_t *p) {

	const int32_t hash = ((int32_t) fabs(p->dist)) & (PLANE_HASHES - 1);

	p->hash_chain = plane_hash[hash];
	plane_hash[hash] = p;
}

/**
 * @brief
 */
static int32_t CreatePlane(const vec3_t normal, double dist) {

	// bad plane
	if (Vec3_Length(normal) < 0.5) {
		return -1;
	}

	// create a new plane
	if (num_planes + 2 > MAX_BSP_PLANES) {
		Com_Error(ERROR_FATAL, "MAX_BSP_PLANES\n");
	}

	plane_t *a = &planes[num_planes++];
	a->normal = normal;
	a->dist = dist;
	a->type = Cm_PlaneTypeForNormal(a->normal);

	plane_t *b = &planes[num_planes++];
	b->normal = Vec3_Negate(normal);
	b->dist = -dist;
	b->type = Cm_PlaneTypeForNormal(b->normal);

	// always put axial planes facing positive first
	if (AXIAL(a)) {
		if (a->normal.x < 0.0 || a->normal.y < 0.0 || a->normal.z < 0.0) {
			plane_t temp = *a;
			*a = *b;
			*b = temp;

			AddPlaneToHash(a);
			AddPlaneToHash(b);
			return num_planes - 1;
		}
	}

	AddPlaneToHash(a);
	AddPlaneToHash(b);
	return num_planes - 2;
}

/**
 * @brief If the specified normal is very close to an axis, align with it.
 */
static vec3_t SnapNormal(const vec3_t normal) {

	vec3_t snapped = normal;

	_Bool snap = false;
	for (int32_t i = 0; i < 3; i++) {
		if (snapped.xyz[i] != 0.0) {
			if (snapped.xyz[i] > -NORMAL_EPSILON && snapped.xyz[i] < NORMAL_EPSILON) {
				snapped.xyz[i] = 0.0;
				snap = true;
			}
		}
	}

	if (snap) {
		snapped = Vec3_Normalize(snapped);
	}

	return snapped;
}

/**
 * @brief
 */
static void SnapPlane(vec3_t *normal, double *dist) {

	*normal = SnapNormal(*normal);

	// snap axial planes to integer distances
	if (Cm_PlaneTypeForNormal(*normal) <= PLANE_Z) {
		const float f = floor(*dist + 0.5);
		if (fabs(*dist - f) < DIST_EPSILON) {
			*dist = f;
		}
	}
}

/**
 * @brief
 */
int32_t FindPlane(const vec3_t normal, double dist) {

	vec3_t snapped = normal;
	SnapPlane(&snapped, &dist);

	const int32_t hash = ((int32_t) fabs(dist)) & (PLANE_HASHES - 1);

	// search the adjacent bins as well
	for (int32_t i = -1; i <= 1; i++) {
		const int32_t h = (hash + i) & (PLANE_HASHES - 1);
		
		const plane_t *p = plane_hash[h];
		while (p) {
			if (PlaneEqual(p, snapped, dist)) {
				return (int32_t) (ptrdiff_t) (p - planes);
			}
			p = p->hash_chain;
		}
	}

	return CreatePlane(snapped, dist);
}

/**
 * @brief
 */
static int32_t PlaneFromPoints(const vec3d_t p0, const vec3d_t p1, const vec3d_t p2) {

	const vec3d_t t1 = Vec3d_Subtract(p0, p1);
	const vec3d_t t2 = Vec3d_Subtract(p2, p1);

	const vec3d_t normal = Vec3d_Normalize(Vec3d_Cross(t1, t2));
	const double dist = Vec3d_Dot(p0, normal);

	return FindPlane(Vec3d_CastVec3(normal), dist);
}

/**
 * @brief
 */
static int32_t BrushContents(const brush_t *b) {

	const brush_side_t *s = &b->sides[0];

	int32_t contents = s->contents;

	for (int32_t i = 1; i < b->num_sides; i++, s++) {

		if ((s->contents & CONTENTS_MASK_VISIBLE) != (contents & CONTENTS_MASK_VISIBLE)) {
			char bits[33], bobs[33];

			SDL_itoa(s->contents & CONTENTS_MASK_VISIBLE, bits, 2);
			SDL_itoa(contents & CONTENTS_MASK_VISIBLE, bobs, 2);

			Mon_SendSelect(MON_WARN, b->entity_num, b->brush_num,
						   va("Mixed face contents: %s expected %s", bits, bobs));
			break;
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
	double dist;
	cm_winding_t *w, *w2;
	vec3_t vec, vec2;
	float d;

	// add the axial planes
	order = 0;
	for (axis = 0; axis < 3; axis++) {
		for (dir = -1; dir <= 1; dir += 2, order++) {
			// see if the plane is already present
			for (i = 0, s = b->sides; i < b->num_sides; i++, s++) {
				if (planes[s->plane_num].normal.xyz[axis] == dir) {
					break;
				}
			}

			if (i == b->num_sides) { // add a new side
				if (num_brush_sides == MAX_BSP_BRUSH_SIDES) {
					Com_Error(ERROR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
				}
				num_brush_sides++;
				b->num_sides++;
				normal = Vec3_Zero();
				normal.xyz[axis] = dir;
				if (dir == 1) {
					dist = b->maxs.xyz[axis];
				} else {
					dist = -b->mins.xyz[axis];
				}
				s->plane_num = FindPlane(normal, dist);
				s->texinfo = b->sides[0].texinfo;
				s->contents = b->sides[0].contents;
				s->bevel = true;
				c_box_bevels++;
			}
			// if the plane is not in it canonical order, swap it
			if (i != order) {
				sidetemp = b->sides[order];
				b->sides[order] = b->sides[i];
				b->sides[i] = sidetemp;

				j = (int32_t) (ptrdiff_t) (b->sides - brush_sides);
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
		s = b->sides + i;
		w = s->winding;
		if (!w) {
			continue;
		}
		for (j = 0; j < w->num_points; j++) {
			k = (j + 1) % w->num_points;
			vec = Vec3_Subtract(w->points[j], w->points[k]);
			if (Vec3_Length(vec) < 0.5) {
				continue;
			}
			vec = Vec3_Normalize(vec);
			vec = SnapNormal(vec);
			for (k = 0; k < 3; k++) {
				if (vec.xyz[k] == -1.0 || vec.xyz[k] == 1.0 ||
					(vec.xyz[k] == 0.0 && vec.xyz[(k + 1) % 3] == 0.0)) {
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
					vec2 = Vec3_Zero();
					vec2.xyz[axis] = dir;
					normal = Vec3_Cross(vec, vec2);
					if (Vec3_Length(normal) < 0.5) {
						continue;
					}
					normal = Vec3_Normalize(normal);
					dist = Vec3_Dot(w->points[j], normal);

					// if all the points on all the sides are
					// behind this plane, it is a proper edge bevel
					for (k = 0; k < b->num_sides; k++) {
						float minBack;

						// if this plane has already been used, skip it
						if (PlaneEqual(&planes[b->sides[k].plane_num], normal, dist)) {
							break;
						}

						w2 = b->sides[k].winding;
						if (!w2) {
							continue;
						}
						minBack = 0.0f;
						for (l = 0; l < w2->num_points; l++) {
							d = Vec3_Dot(w2->points[l], normal) - dist;
							if (d > 0.1) {
								break; // point in front
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

					s2 = &b->sides[b->num_sides++];
					s2->plane_num = FindPlane(normal, dist);
					s2->texinfo = b->sides[0].texinfo;
					s2->contents = b->sides[0].contents;
					s2->bevel = true;

					num_brush_sides++;
					c_edge_bevels++;
				}
			}
		}
	}
}

/**
 * @brief Makes windings for sides and mins / maxs for the brush
 */
static void MakeBrushWindings(brush_t *brush) {

	brush->mins = Vec3_Mins();
	brush->maxs = Vec3_Maxs();

	brush_side_t *side = brush->sides;
	for (int32_t i = 0; i < brush->num_sides; i++, side++) {

		const plane_t *plane = &planes[side->plane_num];
		side->winding = Cm_WindingForPlane(plane->normal, plane->dist);

		const brush_side_t *other = brush->sides;
		for (int32_t j = 0; j < brush->num_sides; j++, other++) {
			if (side == other) {
				continue;
			}
			if (other->plane_num == (side->plane_num ^ 1)) {
				continue;
			}
			const plane_t *plane = &planes[other->plane_num ^ 1];
			Cm_ClipWinding(&side->winding, plane->normal, plane->dist, 0.f);

			if (side->winding == NULL) {
				break;
			}
		}

		if (side->winding) {
			for (int32_t j = 0; j < side->winding->num_points; j++) {
				brush->mins = Vec3_Minf(brush->mins, side->winding->points[j]);
				brush->maxs = Vec3_Maxf(brush->maxs, side->winding->points[j]);
			}
		} else {
			Mon_SendSelect(MON_WARN, brush->entity_num, brush->brush_num, "Malformed brush");
			brush->num_sides = 0;
			break;
		}
	}

	for (int32_t i = 0; i < 3; i++) {
		//IDBUG: all the indexes into the mins and maxs were zero (not using i)
		if (brush->mins.xyz[i] < MIN_WORLD_COORD || brush->maxs.xyz[i] > MAX_WORLD_COORD) {
			Mon_SendSelect(MON_WARN, brush->entity_num, brush->brush_num, "Brush bounds out of range");
			brush->num_sides = 0;
			break;
		}
		if (brush->mins.xyz[i] > MAX_WORLD_COORD || brush->maxs.xyz[i] < MIN_WORLD_COORD) {
			Mon_SendSelect(MON_WARN, brush->entity_num, brush->brush_num, "No visible sides on brush");
			brush->num_sides = 0;
			break;
		}
	}
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
			if (td->value == 0) {
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
		side->contents |= CONTENTS_LADDER | CONTENTS_PLAYER_CLIP;
	} else if (!g_strcmp0(td->name, "common/origin")) {
		side->contents |= CONTENTS_ORIGIN;
	} else if (!g_strcmp0(td->name, "common/skip")) {
		td->flags |= SURF_SKIP;
	} else if (!g_strcmp0(td->name, "common/sky")) {
		td->flags |= SURF_SKY;
	} else if (!g_strcmp0(td->name, "common/trigger")) {
		side->contents |= CONTENTS_PLAYER_CLIP;
	}

	if (side->contents & CONTENTS_MASK_LIQUID) {
		td->flags |= SURF_LIQUID;
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

		brush->sides = &brush_sides[num_brush_sides];
		brush->entity_num = num_entities - 1;
		brush->brush_num = num_brushes - 1 - entity->first_brush;

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

			vec3d_t points[3];

			// read the three point plane definition
			for (int32_t i = 0; i < 3; i++) {

				Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
				if (g_strcmp0(token, "(")) {
					Com_Error(ERROR_FATAL, "Invalid brush %d (%s)\n", num_brushes, token);
				}

				Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_DOUBLE, &points[i].x, 1);
				Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_DOUBLE, &points[i].y, 1);
				Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_DOUBLE, &points[i].z, 1);

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

			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &td.shift.x, 1);
			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &td.shift.y, 1);
			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &td.rotate, 1);
			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &td.scale.x, 1);
			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &td.scale.y, 1);

			if (!Parse_IsEOL(parser)) {
				Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &side->contents, 1);
				Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &td.flags, 1);
				Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &td.value, 1);
			}

			// resolve material-based surface and contents flags
			SetMaterialFlags(side, &td);

			side->surf = td.flags;

			// translucent brushes are inherently details beacuse they can not occlude
			if (side->surf & SURF_MASK_TRANSLUCENT) {
				side->contents |= CONTENTS_TRANSLUCENT | CONTENTS_DETAIL;

				// and translucent solids are actually windows
				if (!(side->contents & CONTENTS_MIST) && !(side->contents & CONTENTS_MASK_LIQUID)) {
					side->contents |= CONTENTS_WINDOW;
				}
			}

			// clip brushes, similarly, are not drawn and therefore can not occlude
			if (side->contents & (CONTENTS_PLAYER_CLIP | CONTENTS_MONSTER_CLIP)) {
				side->contents |= CONTENTS_DETAIL;
			}


			// brushes with no visible or specific contents default to solid
			if (!(side->contents & CONTENTS_MASK_VISIBLE)) {
				if (!(side->contents & (CONTENTS_AREA_PORTAL | CONTENTS_ORIGIN | CONTENTS_MASK_CLIP))) {
					side->contents |= CONTENTS_SOLID;
				}
			}

			// hints are never detail, are visible, and have no contents
			if (side->surf & SURF_HINT) {
				side->visible = true;
				side->contents = 0;
			}

			// skips are never visible and have no contents
			if (side->surf & SURF_SKIP) {
				side->visible = false;
				side->contents = 0;
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
				brush_side_t *side = brush->sides + i;
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
			side = brush->sides + brush->num_sides;
			side->plane_num = plane_num;
			side->texinfo = TexinfoForBrushTexture(&planes[plane_num], &td, Vec3_Zero());

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
		if (no_liquid && (brush->contents & CONTENTS_MASK_LIQUID)) {
			num_brushes--;
			return NULL;
		}

		// create windings for sides and bounds for brush
		MakeBrushWindings(brush);

		// brushes that will not be visible at all will never be used as bsp splitters
		if (brush->contents & (CONTENTS_PLAYER_CLIP | CONTENTS_MONSTER_CLIP)) {
			c_clip_brushes++;
			for (int32_t i = 0; i < brush->num_sides; i++) {
				brush->sides[i].texinfo = TEXINFO_NODE;
			}
		}

		// origin brushes are removed, but they set the rotation origin for the rest of the brushes
		// in the entity. After the entire entity is parsed, the plane_nums and texinfos will be adjusted for
		// the origin brush
		if (brush->contents & CONTENTS_ORIGIN) {

			if (brush->entity_num == 0) {
				Mon_SendSelect(MON_WARN, brush->entity_num, brush->brush_num, "Origin brush in world");
			} else {
				vec3_t origin;
				origin = Vec3_Add(brush->mins, brush->maxs);
				origin = Vec3_Scale(origin, 0.5);

				SetValueForKey(entity, "origin", va("%g %g %g", origin.x, origin.y, origin.z));
			}

			num_brushes--;
			return NULL;
		}

		// add brush bevels, which are required for collision
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

				if (!Parse_Token(parser, PARSE_DEFAULT, e->key, sizeof(e->key))) {
					Com_Error(ERROR_FATAL, "Invalid entity key\n");
				}

				Parse_Token(parser, PARSE_DEFAULT | PARSE_ALLOW_OVERRUN, e->value, sizeof(e->value));
				
				e->next = entity->values;
				entity->values = e;
			}
		}

		const vec3_t origin = VectorForKey(entity, "origin", Vec3_Zero());

		// if there was an origin brush, offset all of the planes and texinfo
		if (!Vec3_Equal(origin, Vec3_Zero())) {
			for (int32_t i = 0; i < entity->num_brushes; i++) {
				brush_t *b = &brushes[entity->first_brush + i];
				for (int32_t j = 0; j < b->num_sides; j++) {

					brush_side_t *s = &b->sides[j];
					plane_t *p = &planes[s->plane_num];

					const double dist = p->dist - Vec3_Dot(p->normal, origin);

					s->plane_num = FindPlane(p->normal, dist);
					s->texinfo = TexinfoForBrushTexture(p, &brush_textures[s - brush_sides], origin);
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

	map_mins = Vec3_Mins();
	map_maxs = Vec3_Maxs();

	for (int32_t i = 0; i < entities[0].num_brushes; i++) {
		if (brushes[i].mins.x > MAX_WORLD_COORD) {
			continue; // no valid points
		}
		map_mins = Vec3_Minf(map_mins, brushes[i].mins);
		map_maxs = Vec3_Maxf(map_maxs, brushes[i].maxs);
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
				map_mins.x, map_mins.y, map_mins.z, map_maxs.x, map_maxs.y, map_maxs.z);

	Fs_Free(buffer);
}
