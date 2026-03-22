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

#include "bsp.h"
#include "map.h"
#include "material.h"
#include "patch.h"
#include "tree.h"

int32_t num_patches;
patch_t patches[MAX_PATCHES];

static void EvaluatePatch(const patch_control_point_t cp[3][3],
              float s, float t,
              vec3_t *out_position,
              vec2_t *out_st,
              vec3_t *out_normal);

/**
 * @brief Parses a patchDef2 block from the map file.
 * @details Expected format:
 * ```
 *   patchDef2
 *   {
 *    texture_name
 *    ( width height 0 0 0 )
 *    (
 *    ( ( x y z u v ) ( x y z u v ) ... )
 *    ( ( x y z u v ) ( x y z u v ) ... )
 *    ...
 *    )
 *   }
 * ```
 * The opening `{` of the brush has already been consumed, and the `patchDef2`
 * token has been peeked but not consumed.
 */
patch_t *ParsePatch(parser_t *parser, int32_t entity_num) {
  char token[MAX_TOKEN_CHARS];

  // consume "patchDef2"
  Parse_SkipToken(parser, PARSE_DEFAULT);

  // consume "{"
  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (g_strcmp0(token, "{")) {
    Com_Error(ERROR_FATAL, "Expected '{' after patchDef2, got '%s'\n", token);
  }

  if (num_patches == MAX_PATCHES) {
    Com_Error(ERROR_FATAL, "MAX_PATCHES\n");
  }

  patch_t *patch = &patches[num_patches];
  memset(patch, 0, sizeof(*patch));
  patch->entity = entity_num;

  // read texture name
  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (strlen(token) > sizeof(patch->texture) - 1) {
    Com_Error(ERROR_FATAL, "Patch texture name \"%s\" is too long.\n", token);
  }
  g_strlcpy(patch->texture, token, sizeof(patch->texture));

  // read "( rows cols 0 0 0 )" — rows = outer dimension, cols = inner dimension
  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (g_strcmp0(token, "(")) {
    Com_Error(ERROR_FATAL, "Expected '(' for patch dimensions, got '%s'\n", token);
  }

  int32_t num_rows, num_cols;
  Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &num_rows, 1);
  Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &num_cols, 1);

  // skip the 3 reserved values
  int32_t reserved;
  Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &reserved, 1);
  Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &reserved, 1);
  Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &reserved, 1);

  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (g_strcmp0(token, ")")) {
    Com_Error(ERROR_FATAL, "Expected ')' after patch dimensions, got '%s'\n", token);
  }

  // Store as width (columns) x height (rows) internally
  patch->width = num_cols;
  patch->height = num_rows;

  if (patch->width < 3 || patch->height < 3 ||
    patch->width > MAX_PATCH_WIDTH || patch->height > MAX_PATCH_HEIGHT ||
    (patch->width & 1) == 0 || (patch->height & 1) == 0) {
    Com_Error(ERROR_FATAL, "Invalid patch dimensions %dx%d\n", patch->width, patch->height);
  }

  // read "(" to begin control point grid
  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (g_strcmp0(token, "(")) {
    Com_Error(ERROR_FATAL, "Expected '(' for patch control points, got '%s'\n", token);
  }

  // read rows of control points (num_rows outer lines, num_cols inner points)
  for (int32_t row = 0; row < num_rows; row++) {

    // read "(" to begin row
    Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
    if (g_strcmp0(token, "(")) {
      Com_Error(ERROR_FATAL, "Expected '(' for patch row %d, got '%s'\n", row, token);
    }

    for (int32_t col = 0; col < num_cols; col++) {
      patch_control_point_t *cp = &patch->control_points[row * num_cols + col];

      // read "("
      Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
      if (g_strcmp0(token, "(")) {
        Com_Error(ERROR_FATAL, "Expected '(' for control point [%d][%d], got '%s'\n", row, col, token);
      }

      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &cp->position.x, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &cp->position.y, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &cp->position.z, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &cp->st.x, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &cp->st.y, 1);

      // read ")"
      Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
      if (g_strcmp0(token, ")")) {
        Com_Error(ERROR_FATAL, "Expected ')' for control point [%d][%d], got '%s'\n", row, col, token);
      }
    }

    // read ")" to end row
    Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
    if (g_strcmp0(token, ")")) {
      Com_Error(ERROR_FATAL, "Expected ')' to end patch row %d, got '%s'\n", row, token);
    }
  }

  // read ")" to end control point grid
  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (g_strcmp0(token, ")")) {
    Com_Error(ERROR_FATAL, "Expected ')' to end patch control points, got '%s'\n", token);
  }

  // read "}" to end patchDef2
  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (g_strcmp0(token, "}")) {
    Com_Error(ERROR_FATAL, "Expected '}' to end patchDef2, got '%s'\n", token);
  }

  // read "}" to end brush containing the patch
  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (g_strcmp0(token, "}")) {
    Com_Error(ERROR_FATAL, "Expected '}' to end patch brush, got '%s'\n", token);
  }

  // resolve material
  patch->material = FindMaterial(patch->texture);
  patch->contents = CONTENTS_SOLID | CONTENTS_DETAIL;
  patch->surface = 0;

  num_patches++;
  return patch;
}

#define PATCH_COLLISION_FACET_SIZE 32.f
#define PATCH_COLLISION_THICKNESS 16.f

/**
 * @brief Creates a single collision brush from a triangle on the patch surface.
 * @param entity The entity to add the brush to.
 * @param v Triangle vertices (3 points on the surface).
 * @param normal The outward-facing surface normal.
 */
static void EmitPatchCollisionBrush(entity_t *entity,
                                    const vec3_t v[3],
                                    const vec3_t normal) {

  // Check for degenerate triangle
  const vec3_t edge1 = Vec3_Subtract(v[1], v[0]);
  const vec3_t edge2 = Vec3_Subtract(v[2], v[0]);
  const vec3_t cross = Vec3_Cross(edge1, edge2);
  if (Vec3_Length(cross) < 1.f) {
    return;
  }

  if (num_brushes >= MAX_BSP_BRUSHES) {
    Com_Error(ERROR_FATAL, "MAX_BSP_BRUSHES\n");
  }
  if (num_brush_sides + 5 >= MAX_BSP_BRUSH_SIDES) {
    Com_Error(ERROR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
  }

  const int32_t caulk_material = FindMaterial("common/caulk");

  brush_t *brush = &brushes[num_brushes];
  memset(brush, 0, sizeof(*brush));
  brush->entity = (int32_t) (entity - entities);
  brush->brush = num_brushes - entity->first_brush;
  brush->brush_sides = &brush_sides[num_brush_sides];

  // Front face: derive from triangle vertices so all 3 lie exactly on the plane.
  // Flip to match the outward Bézier surface normal direction.
  const bool flip = Vec3_Dot(cross, normal) < 0.f;
  const vec3_t front_normal = flip ? Vec3_Negate(Vec3_Normalize(cross))
                                   : Vec3_Normalize(cross);
  const double front_dist = Vec3_Dot(front_normal, v[0]);

  // Back face: opposite normal, offset by thickness
  const vec3_t back_normal = Vec3_Negate(front_normal);
  const double back_dist = -front_dist + PATCH_COLLISION_THICKNESS;

  // Side planes: for each edge, the outward normal is edge × front_normal.
  // When we flipped front_normal, we must also flip edges to keep side normals outward.
  const float ws = flip ? -1.f : 1.f;
  const vec3_t edges[3] = {
    Vec3_Scale(Vec3_Subtract(v[1], v[0]), ws),
    Vec3_Scale(Vec3_Subtract(v[2], v[1]), ws),
    Vec3_Scale(Vec3_Subtract(v[0], v[2]), ws),
  };

  int32_t num_sides = 0;
  brush_side_t *side;

  // Front
  side = &brush->brush_sides[num_sides];
  memset(side, 0, sizeof(*side));
  side->plane = FindPlane(front_normal, front_dist);
  side->contents = CONTENTS_SOLID | CONTENTS_DETAIL;
  side->surface = SURF_NO_DRAW;
  side->material = caulk_material;
  g_strlcpy(side->texture, "common/caulk", sizeof(side->texture));
  side->scale = Vec2(1.f, 1.f);
  num_sides++;

  // Back
  side = &brush->brush_sides[num_sides];
  memset(side, 0, sizeof(*side));
  side->plane = FindPlane(back_normal, back_dist);
  side->contents = CONTENTS_SOLID | CONTENTS_DETAIL;
  side->surface = SURF_NO_DRAW;
  side->material = caulk_material;
  g_strlcpy(side->texture, "common/caulk", sizeof(side->texture));
  side->scale = Vec2(1.f, 1.f);
  num_sides++;

  // 3 side planes
  for (int32_t i = 0; i < 3; i++) {
    vec3_t side_normal = Vec3_Cross(edges[i], front_normal);
    const float len = Vec3_Length(side_normal);
    if (len < 0.1f) {
      continue;
    }
    side_normal = Vec3_Scale(side_normal, 1.f / len);
    const double side_dist = Vec3_Dot(side_normal, v[i]);

    side = &brush->brush_sides[num_sides];
    memset(side, 0, sizeof(*side));
    side->plane = FindPlane(side_normal, side_dist);
    side->contents = CONTENTS_SOLID | CONTENTS_DETAIL;
    side->surface = SURF_NO_DRAW;
    side->material = caulk_material;
    g_strlcpy(side->texture, "common/caulk", sizeof(side->texture));
    side->scale = Vec2(1.f, 1.f);
    num_sides++;
  }

  if (num_sides < 4) {
    return;
  }

  brush->num_brush_sides = num_sides;
  num_brush_sides += num_sides;
  num_brushes++;

  brush->contents = CONTENTS_SOLID | CONTENTS_DETAIL;

  MakeBrushWindings(brush);

  if (brush->num_brush_sides) {
    AddBrushBevels(brush);
    entity->num_brushes++;
    entity->num_brush_sides += brush->num_brush_sides;
    entity->bounds = Box3_Union(entity->bounds, brush->bounds);
  }
}

/**
 * @brief Generates collision brushes for a patch by tessellating at a coarse
 * resolution and emitting caulk brushes for each quad cell.
 */
void EmitPatchCollisionBrushes(patch_t *patch, entity_t *entity) {

  if (patch->material < 0) {
    return;
  }

  const int32_t num_sub_patches_s = (patch->width - 1) / 2;
  const int32_t num_sub_patches_t = (patch->height - 1) / 2;

  for (int32_t sub_t = 0; sub_t < num_sub_patches_t; sub_t++) {
    for (int32_t sub_s = 0; sub_s < num_sub_patches_s; sub_s++) {

      // Extract the 3×3 control point sub-grid
      patch_control_point_t sub_cp[3][3];
      for (int32_t row = 0; row < 3; row++) {
        for (int32_t col = 0; col < 3; col++) {
          const int32_t src_row = sub_t * 2 + row;
          const int32_t src_col = sub_s * 2 + col;
          sub_cp[row][col] = patch->control_points[src_row * patch->width + src_col];
        }
      }

      // Determine subdivision count based on sub-patch size
      vec3_t corner00, corner10, corner01;
      vec2_t st_dummy;
      EvaluatePatch(sub_cp, 0.f, 0.f, &corner00, &st_dummy, NULL);
      EvaluatePatch(sub_cp, 1.f, 0.f, &corner10, &st_dummy, NULL);
      EvaluatePatch(sub_cp, 0.f, 1.f, &corner01, &st_dummy, NULL);

      const float size_s = Vec3_Distance(corner00, corner10);
      const float size_t = Vec3_Distance(corner00, corner01);

      const int32_t subdivs_s = Maxi(1, (int32_t) (size_s / PATCH_COLLISION_FACET_SIZE));
      const int32_t subdivs_t = Maxi(1, (int32_t) (size_t / PATCH_COLLISION_FACET_SIZE));

      // Tessellate and emit a brush for each quad cell
      for (int32_t j = 0; j < subdivs_t; j++) {
        for (int32_t i = 0; i < subdivs_s; i++) {

          const float s0 = (float) i / (float) subdivs_s;
          const float s1 = (float) (i + 1) / (float) subdivs_s;
          const float t0 = (float) j / (float) subdivs_t;
          const float t1 = (float) (j + 1) / (float) subdivs_t;

          vec3_t verts[4], normal;
          vec2_t st;
          vec3_t n00, n10, n01, n11;

          EvaluatePatch(sub_cp, s0, t0, &verts[0], &st, &n00);
          EvaluatePatch(sub_cp, s1, t0, &verts[1], &st, &n10);
          EvaluatePatch(sub_cp, s0, t1, &verts[2], &st, &n01);
          EvaluatePatch(sub_cp, s1, t1, &verts[3], &st, &n11);

          // Average normal for extrusion direction
          normal = Vec3_Normalize(Vec3_Add(Vec3_Add(n00, n10), Vec3_Add(n01, n11)));

          // Split quad into two triangles and emit a prism brush for each
          const vec3_t tri0[3] = { verts[0], verts[1], verts[2] };
          const vec3_t tri1[3] = { verts[1], verts[3], verts[2] };
          EmitPatchCollisionBrush(entity, tri0, normal);
          EmitPatchCollisionBrush(entity, tri1, normal);
        }
      }
    }
  }
}

/**
 * @brief Evaluates a biquadratic Bézier curve at parameter t.
 * @param p0 The first control point.
 * @param p1 The second control point (midpoint).
 * @param p2 The third control point.
 * @param t The parameter [0, 1].
 * @return The interpolated value.
 */
static float BezierQuadratic(float p0, float p1, float p2, float t) {
  const float it = 1.f - t;
  return it * it * p0 + 2.f * it * t * p1 + t * t * p2;
}

/**
 * @brief Evaluates the derivative of a biquadratic Bézier curve at parameter t.
 */
static float BezierQuadraticDeriv(float p0, float p1, float p2, float t) {
  return 2.f * (1.f - t) * (p1 - p0) + 2.f * t * (p2 - p1);
}

/**
 * @brief Evaluates a biquadratic Bézier surface patch at parameter (s, t).
 */
static void EvaluatePatch(const patch_control_point_t cp[3][3],
              float s, float t,
              vec3_t *out_position,
              vec2_t *out_st,
              vec3_t *out_normal) {

  // Evaluate the 3×3 biquadratic Bézier surface
  for (int32_t k = 0; k < 3; k++) {
    // Interpolate along rows (t direction) to get 3 intermediate points
    const float r0 = BezierQuadratic(cp[0][0].position.xyz[k],
                      cp[1][0].position.xyz[k],
                      cp[2][0].position.xyz[k], t);
    const float r1 = BezierQuadratic(cp[0][1].position.xyz[k],
                      cp[1][1].position.xyz[k],
                      cp[2][1].position.xyz[k], t);
    const float r2 = BezierQuadratic(cp[0][2].position.xyz[k],
                      cp[1][2].position.xyz[k],
                      cp[2][2].position.xyz[k], t);
    // Then interpolate along columns (s direction)
    out_position->xyz[k] = BezierQuadratic(r0, r1, r2, s);
  }

  // Interpolate texture coordinates
  for (int32_t k = 0; k < 2; k++) {
    const float r0 = BezierQuadratic(cp[0][0].st.xy[k],
                      cp[1][0].st.xy[k],
                      cp[2][0].st.xy[k], t);
    const float r1 = BezierQuadratic(cp[0][1].st.xy[k],
                      cp[1][1].st.xy[k],
                      cp[2][1].st.xy[k], t);
    const float r2 = BezierQuadratic(cp[0][2].st.xy[k],
                      cp[1][2].st.xy[k],
                      cp[2][2].st.xy[k], t);
    out_st->xy[k] = BezierQuadratic(r0, r1, r2, s);
  }

  if (out_normal) {
    // Compute partial derivatives for normal
    vec3_t ds, dt;
    for (int32_t k = 0; k < 3; k++) {
      const float r0 = BezierQuadratic(cp[0][0].position.xyz[k],
                        cp[1][0].position.xyz[k],
                        cp[2][0].position.xyz[k], t);
      const float r1 = BezierQuadratic(cp[0][1].position.xyz[k],
                        cp[1][1].position.xyz[k],
                        cp[2][1].position.xyz[k], t);
      const float r2 = BezierQuadratic(cp[0][2].position.xyz[k],
                        cp[1][2].position.xyz[k],
                        cp[2][2].position.xyz[k], t);
      ds.xyz[k] = BezierQuadraticDeriv(r0, r1, r2, s);

      const float c0 = BezierQuadratic(cp[0][0].position.xyz[k],
                        cp[0][1].position.xyz[k],
                        cp[0][2].position.xyz[k], s);
      const float c1 = BezierQuadratic(cp[1][0].position.xyz[k],
                        cp[1][1].position.xyz[k],
                        cp[1][2].position.xyz[k], s);
      const float c2 = BezierQuadratic(cp[2][0].position.xyz[k],
                        cp[2][1].position.xyz[k],
                        cp[2][2].position.xyz[k], s);
      dt.xyz[k] = BezierQuadraticDeriv(c0, c1, c2, t);
    }

    *out_normal = Vec3_Cross(dt, ds);
    const float len = Vec3_Length(*out_normal);
    if (len > 0.f) {
      *out_normal = Vec3_Scale(*out_normal, 1.f / len);
    } else {
      *out_normal = Vec3(0.f, 0.f, 1.f);
    }
  }
}

/**
 * @brief Pre-tessellates all patches belonging to the given entity.
 * @details Tessellates each 3×3 sub-patch into patch_face_t structures stored
 * on the patch_t, and updates the patch bounds accordingly. These precomputed
 * faces are later used when emitting BSP geometry.
 */
void TessellatePatches(int32_t entity_num) {

  const int32_t subdivisions = PATCH_SUBDIVISIONS;

  for (int32_t p = 0; p < num_patches; p++) {
    patch_t *patch = &patches[p];

    if (patch->entity != entity_num) {
      continue;
    }

    if (patch->material < 0) {
      continue;
    }

    const int32_t num_sub_patches_s = (patch->width - 1) / 2;
    const int32_t num_sub_patches_t = (patch->height - 1) / 2;

    patch->num_faces = num_sub_patches_s * num_sub_patches_t;
    patch->faces = Mem_TagMalloc(patch->num_faces * sizeof(patch_face_t), MEM_TAG_PATCH);
    patch->bounds = Box3_Null();

    int32_t face_index = 0;
    for (int32_t sub_t = 0; sub_t < num_sub_patches_t; sub_t++) {
      for (int32_t sub_s = 0; sub_s < num_sub_patches_s; sub_s++) {

        // Extract the 3×3 control point sub-grid
        patch_control_point_t sub_cp[3][3];
        for (int32_t row = 0; row < 3; row++) {
          for (int32_t col = 0; col < 3; col++) {
            const int32_t src_row = sub_t * 2 + row;
            const int32_t src_col = sub_s * 2 + col;
            sub_cp[row][col] = patch->control_points[src_row * patch->width + src_col];
          }
        }

        patch_face_t *pf = &patch->faces[face_index++];
        memset(pf, 0, sizeof(*pf));
        pf->patch = patch;
        pf->bounds = Box3_Null();

        const int32_t verts_per_edge = subdivisions + 1;

        // Tessellate: generate (subdivisions+1)² vertices
        for (int32_t j = 0; j <= subdivisions; j++) {
          const float t = (float) j / (float) subdivisions;

          for (int32_t i = 0; i <= subdivisions; i++) {
            const float s = (float) i / (float) subdivisions;

            bsp_vertex_t *v = &pf->vertexes[pf->num_vertexes];
            memset(v, 0, sizeof(*v));

            vec3_t normal;
            vec2_t st;
            EvaluatePatch(sub_cp, s, t, &v->position, &st, &normal);

            v->normal = normal;
            v->diffusemap.x = st.x;
            v->diffusemap.y = st.y;
            v->color = Color32(255, 255, 255, 255);

            pf->bounds = Box3_Append(pf->bounds, v->position);

            pf->num_vertexes++;
          }
        }

        assert(pf->num_vertexes == verts_per_edge * verts_per_edge);

        // Generate triangle elements (local 0-based indices)
        for (int32_t j = 0; j < subdivisions; j++) {
          for (int32_t i = 0; i < subdivisions; i++) {
            const int32_t base = j * verts_per_edge + i;

            pf->elements[pf->num_elements++] = base;
            pf->elements[pf->num_elements++] = base + verts_per_edge + 1;
            pf->elements[pf->num_elements++] = base + verts_per_edge;

            pf->elements[pf->num_elements++] = base;
            pf->elements[pf->num_elements++] = base + 1;
            pf->elements[pf->num_elements++] = base + verts_per_edge + 1;
          }
        }

        patch->bounds = Box3_Union(patch->bounds, pf->bounds);
      }
    }
  }
}

/**
 * @brief Assigns a patch face to the deepest BSP node containing its center.
 */
static void AssignPatchFaceToNode_r(node_t *node, patch_face_t *pf) {

  if (node->plane == PLANE_LEAF) {
    return;
  }

  const vec3_t center = Box3_Center(pf->bounds);

  for (int32_t i = 0; i < 2; i++) {
    if (node->children[i]->plane != PLANE_LEAF &&
        Box3_ContainsPoint(node->children[i]->bounds, center)) {
      AssignPatchFaceToNode_r(node->children[i], pf);
      return;
    }
  }

  pf->next = node->patch_faces;
  node->patch_faces = pf;
}

/**
 * @brief Assigns pre-tessellated patch faces to BSP tree nodes.
 */
void AssignPatchFacesToNodes(node_t *head_node, int32_t entity_num) {

  for (int32_t p = 0; p < num_patches; p++) {
    patch_t *patch = &patches[p];

    if (patch->entity != entity_num) {
      continue;
    }

    for (int32_t f = 0; f < patch->num_faces; f++) {
      AssignPatchFaceToNode_r(head_node, &patch->faces[f]);
    }
  }
}

/**
 * @brief Frees pre-tessellated patch face data for the given entity.
 */
void FreePatchFaces(int32_t entity_num) {

  for (int32_t p = 0; p < num_patches; p++) {
    patch_t *patch = &patches[p];

    if (patch->entity != entity_num) {
      continue;
    }

    Mem_Free(patch->faces);
    patch->faces = NULL;
    patch->num_faces = 0;
  }
}

/**
 * @brief Emits patches belonging to the given model to the BSP patches lump.
 */
void EmitPatches(const bsp_model_t *mod) {

  const int32_t entity_num = mod->entity;

  for (int32_t p = 0; p < num_patches; p++) {
    patch_t *patch = &patches[p];

    if (patch->entity != entity_num) {
      continue;
    }

    if (patch->material < 0) {
      continue;
    }

    if (bsp_file.num_patches >= MAX_BSP_PATCHES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_PATCHES\n");
    }

    patch->out = &bsp_file.patches[bsp_file.num_patches];
    memset(patch->out, 0, sizeof(*patch->out));
    bsp_file.num_patches++;

    patch->out->entity = patch->entity;
    patch->out->material = patch->material;
    patch->out->contents = patch->contents;
    patch->out->surface = patch->surface;
    patch->out->width = patch->width;
    patch->out->height = patch->height;

    // Set the patch index on all emitted BSP faces, and find first_face/num_faces
    const int32_t patch_index = (int32_t) (patch->out - bsp_file.patches);
    int32_t first_face = -1;
    int32_t num_faces = 0;

    for (int32_t f = 0; f < patch->num_faces; f++) {
      if (patch->faces[f].out) {
        patch->faces[f].out->patch = patch_index;
        const int32_t face_index = (int32_t) (patch->faces[f].out - bsp_file.faces);
        if (first_face < 0 || face_index < first_face) {
          first_face = face_index;
        }
        num_faces++;
      }
    }

    patch->out->first_face = first_face;
    patch->out->num_faces = num_faces;

    const int32_t num_points = patch->width * patch->height;
    for (int32_t i = 0; i < num_points; i++) {
      patch->out->control_points[i].position = patch->control_points[i].position;
      patch->out->control_points[i].st = patch->control_points[i].st;
    }
  }
}
