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
 * @brief Frees pre-tessellated patch face data for the given entity.
 */
void FreePatchFaces(int32_t entity_num) {

  for (int32_t p = 0; p < num_patches; p++) {
    patch_t *patch = &patches[p];

    if (patch->entity != entity_num) {
      continue;
    }

    if (patch->faces) {
      Mem_Free(patch->faces);
      patch->faces = NULL;
      patch->num_faces = 0;
    }
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

    if (patch->num_bsp_faces == 0) {
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
    patch->out->first_face = patch->first_bsp_face;
    patch->out->num_faces = patch->num_bsp_faces;

    // Set the patch index on all emitted BSP faces for this patch
    const int32_t patch_index = (int32_t) (patch->out - bsp_file.patches);
    for (int32_t f = 0; f < patch->num_bsp_faces; f++) {
      bsp_file.faces[patch->first_bsp_face + f].patch = patch_index;
    }

    const int32_t num_points = patch->width * patch->height;
    for (int32_t i = 0; i < num_points; i++) {
      patch->out->control_points[i].position = patch->control_points[i].position;
      patch->out->control_points[i].st = patch->control_points[i].st;
    }
  }
}
