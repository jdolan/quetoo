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

int32_t num_patches;
patch_t patches[MAX_PATCHES];

/**
 * @brief Finds the nearest interior BSP node containing the given point.
 */
static int32_t FindNodeForPoint(int32_t head_node, const vec3_t point) {

  int32_t node_index = head_node;
  int32_t last_node = head_node;

  while (node_index >= 0) {
    last_node = node_index;

    const bsp_node_t *node = &bsp_file.nodes[node_index];
    const bsp_plane_t *plane = &bsp_file.planes[node->plane];
    const float dist = Vec3_Dot(plane->normal, point) - plane->dist;

    if (dist >= 0.f) {
      node_index = node->children[0];
    } else {
      node_index = node->children[1];
    }
  }

  return last_node;
}

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
 * @brief Emits tessellated patch faces for all patches belonging to the given entity.
 */
void EmitPatchFaces(bsp_model_t *mod) {

  mod->first_patch_face = bsp_file.num_faces;

  const int32_t subdivisions = PATCH_SUBDIVISIONS;

  for (int32_t p = 0; p < num_patches; p++) {
    patch_t *patch = &patches[p];

    if (patch->entity != mod->entity) {
      continue;
    }

    if (patch->material < 0) {
      continue;
    }

    // Emit a synthetic brush side for this patch
    if (bsp_file.num_brush_sides >= MAX_BSP_BRUSH_SIDES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
    }

    bsp_brush_side_t *bsp_side = &bsp_file.brush_sides[bsp_file.num_brush_sides];
    memset(bsp_side, 0, sizeof(*bsp_side));
    bsp_side->plane = 0;
    bsp_side->material = patch->material;
    bsp_side->contents = patch->contents;
    bsp_side->surface = patch->surface;

    const int32_t brush_side_index = bsp_file.num_brush_sides;
    bsp_file.num_brush_sides++;

    // Emit a synthetic brush for this patch
    if (bsp_file.num_brushes >= MAX_BSP_BRUSHES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_BRUSHES\n");
    }

    bsp_brush_t *bsp_brush = &bsp_file.brushes[bsp_file.num_brushes];
    memset(bsp_brush, 0, sizeof(*bsp_brush));
    bsp_brush->entity = patch->entity;
    bsp_brush->contents = patch->contents;
    bsp_brush->first_brush_side = brush_side_index;
    bsp_brush->num_brush_sides = 1;
    bsp_brush->bounds = Box3_Null();
    bsp_file.num_brushes++;

    patch->first_face = bsp_file.num_faces;

    // Process each 3×3 sub-patch in the control grid
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

        // Emit a face for this sub-patch
        if (bsp_file.num_faces >= MAX_BSP_FACES) {
          Com_Error(ERROR_FATAL, "MAX_BSP_FACES\n");
        }

        bsp_face_t *out = &bsp_file.faces[bsp_file.num_faces];
        memset(out, 0, sizeof(*out));
        bsp_file.num_faces++;

        out->brush_side = brush_side_index;
        out->plane = -1;
        out->bounds = Box3_Null();
        out->first_vertex = bsp_file.num_vertexes;

        // Tessellate: generate (subdivisions+1)² vertices
        const int32_t verts_per_edge = subdivisions + 1;

        for (int32_t j = 0; j <= subdivisions; j++) {
          const float t = (float) j / (float) subdivisions;

          for (int32_t i = 0; i <= subdivisions; i++) {
            const float s = (float) i / (float) subdivisions;

            if (bsp_file.num_vertexes >= MAX_BSP_VERTEXES) {
              Com_Error(ERROR_FATAL, "MAX_BSP_VERTEXES\n");
            }

            bsp_vertex_t *v = &bsp_file.vertexes[bsp_file.num_vertexes];
            memset(v, 0, sizeof(*v));

            vec3_t normal;
            vec2_t st;
            EvaluatePatch(sub_cp, s, t, &v->position, &st, &normal);

            v->normal = normal;
            v->diffusemap.x = st.x;
            v->diffusemap.y = st.y;
            v->color = Color32(255, 255, 255, 255);

            out->bounds = Box3_Append(out->bounds, v->position);
            bsp_brush->bounds = Box3_Append(bsp_brush->bounds, v->position);

            bsp_file.num_vertexes++;
          }
        }

        out->num_vertexes = verts_per_edge * verts_per_edge;

        out->node = FindNodeForPoint(mod->head_node, Box3_Center(out->bounds));

        // Emit triangle elements
        out->first_element = bsp_file.num_elements;

        for (int32_t j = 0; j < subdivisions; j++) {
          for (int32_t i = 0; i < subdivisions; i++) {
            const int32_t base = out->first_vertex + j * verts_per_edge + i;

            if (bsp_file.num_elements + 6 >= MAX_BSP_ELEMENTS) {
              Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
            }

            // Triangle 1
            bsp_file.elements[bsp_file.num_elements++] = base;
            bsp_file.elements[bsp_file.num_elements++] = base + verts_per_edge + 1;
            bsp_file.elements[bsp_file.num_elements++] = base + verts_per_edge;

            // Triangle 2
            bsp_file.elements[bsp_file.num_elements++] = base;
            bsp_file.elements[bsp_file.num_elements++] = base + 1;
            bsp_file.elements[bsp_file.num_elements++] = base + verts_per_edge + 1;
          }
        }

        out->num_elements = subdivisions * subdivisions * 6;
      }
    }

    patch->num_faces = bsp_file.num_faces - patch->first_face;
  }

  mod->num_patch_faces = bsp_file.num_faces - mod->first_patch_face;
}

/**
 * @brief Emits patches belonging to the given model to the BSP patches lump.
 */
void EmitPatches(const bsp_model_t *mod) {

  const int32_t entity_num = mod->entity;

  for (int32_t p = 0; p < num_patches; p++) {
    const patch_t *patch = &patches[p];

    if (patch->entity != entity_num) {
      continue;
    }

    if (patch->material < 0) {
      continue;
    }

    if (patch->num_faces == 0) {
      continue;
    }

    if (bsp_file.num_patches >= MAX_BSP_PATCHES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_PATCHES\n");
    }

    bsp_patch_t *out = &bsp_file.patches[bsp_file.num_patches];
    memset(out, 0, sizeof(*out));
    bsp_file.num_patches++;

    out->entity = patch->entity;
    out->material = patch->material;
    out->contents = patch->contents;
    out->surface = patch->surface;
    out->width = patch->width;
    out->height = patch->height;
    out->first_face = patch->first_face;
    out->num_faces = patch->num_faces;

    const int32_t num_points = patch->width * patch->height;
    for (int32_t i = 0; i < num_points; i++) {
      out->control_points[i].position = patch->control_points[i].position;
      out->control_points[i].st = patch->control_points[i].st;
    }
  }
}
