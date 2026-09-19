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

int32_t numPatches;
Patch patches[MAX_PATCHES];

static void EvaluatePatch(const PatchControlPoint cp[3][3],
              float s, float t,
              Vec3 *outPosition,
              Vec2 *outSt,
              Vec3 *outNormal);

/**
 * @brief Parses a patchDef2 block from the map file.
 * @details Expected format:
 * ``
 *   patchDef2
 *   {
 *    `texture_name`
 *    ( width height 0 0 0 )
 *    (
 *    ( ( x y z u v ) ( x y z u v ) ... )
 *    ( ( x y z u v ) ( x y z u v ) ... )
 *    ...
 *    )
 *   }
 * ``
 * The opening `{` of the brush has already been consumed, and the `patchDef2`
 * token has been peeked but not consumed.
 */
Patch *ParsePatch(Parser *parser, int32_t entityNum) {
  char token[MAX_TOKEN_CHARS];

  // consume "patchDef2"
  Parse_SkipToken(parser, PARSE_DEFAULT);

  // consume "{"
  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (q_strcmp(token, "{")) {
    Com_Error(ERROR_FATAL, "Expected '{' after patchDef2, got '%s'\n", token);
  }

  if (numPatches == MAX_PATCHES) {
    Com_Error(ERROR_FATAL, "MAX_PATCHES\n");
  }

  Patch *patch = &patches[numPatches];
  memset(patch, 0, sizeof(*patch));
  patch->entity = entityNum;

  // read texture name
  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (q_strlen(token) > sizeof(patch->texture) - 1) {
    Com_Error(ERROR_FATAL, "Patch texture name \"%s\" is too long.\n", token);
  }
  q_strlcpy(patch->texture, token, sizeof(patch->texture));

  // read "( rows cols 0 0 0 )" — rows = outer dimension, cols = inner dimension
  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (q_strcmp(token, "(")) {
    Com_Error(ERROR_FATAL, "Expected '(' for patch dimensions, got '%s'\n", token);
  }

  int32_t numRows, numCols;
  Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &numRows, 1);
  Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &numCols, 1);

  // skip the 3 reserved values
  int32_t reserved;
  Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &reserved, 1);
  Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &reserved, 1);
  Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &reserved, 1);

  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (q_strcmp(token, ")")) {
    Com_Error(ERROR_FATAL, "Expected ')' after patch dimensions, got '%s'\n", token);
  }

  // Store as width (columns) x height (rows) internally
  patch->width = numCols;
  patch->height = numRows;

  if (patch->width < 3 || patch->height < 3 ||
    patch->width > MAX_PATCH_WIDTH || patch->height > MAX_PATCH_HEIGHT ||
    (patch->width & 1) == 0 || (patch->height & 1) == 0) {
    Com_Error(ERROR_FATAL, "Invalid patch dimensions %dx%d\n", patch->width, patch->height);
  }

  // read "(" to begin control point grid
  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (q_strcmp(token, "(")) {
    Com_Error(ERROR_FATAL, "Expected '(' for patch control points, got '%s'\n", token);
  }

  // read rows of control points (num_rows outer lines, num_cols inner points)
  for (int32_t row = 0; row < numRows; row++) {

    // read "(" to begin row
    Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
    if (q_strcmp(token, "(")) {
      Com_Error(ERROR_FATAL, "Expected '(' for patch row %d, got '%s'\n", row, token);
    }

    for (int32_t col = 0; col < numCols; col++) {
      PatchControlPoint *cp = &patch->controlPoints[row * numCols + col];

      // read "("
      Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
      if (q_strcmp(token, "(")) {
        Com_Error(ERROR_FATAL, "Expected '(' for control point [%d][%d], got '%s'\n", row, col, token);
      }

      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &cp->position.x, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &cp->position.y, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &cp->position.z, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &cp->st.x, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &cp->st.y, 1);

      // read ")"
      Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
      if (q_strcmp(token, ")")) {
        Com_Error(ERROR_FATAL, "Expected ')' for control point [%d][%d], got '%s'\n", row, col, token);
      }
    }

    // read ")" to end row
    Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
    if (q_strcmp(token, ")")) {
      Com_Error(ERROR_FATAL, "Expected ')' to end patch row %d, got '%s'\n", row, token);
    }
  }

  // read ")" to end control point grid
  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (q_strcmp(token, ")")) {
    Com_Error(ERROR_FATAL, "Expected ')' to end patch control points, got '%s'\n", token);
  }

  // read "}" to end patchDef2
  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (q_strcmp(token, "}")) {
    Com_Error(ERROR_FATAL, "Expected '}' to end patchDef2, got '%s'\n", token);
  }

  // read "}" to end brush containing the patch
  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
  if (q_strcmp(token, "}")) {
    Com_Error(ERROR_FATAL, "Expected '}' to end patch brush, got '%s'\n", token);
  }

  // resolve material
  patch->material = LoadMaterial(patch->texture);
  patch->contents = CONTENTS_SOLID | CONTENTS_DETAIL;
  patch->surface = 0;

  numPatches++;
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
static void EmitPatchCollisionBrush(Entity *entity,
                                    const Vec3 v[3],
                                    const Vec3 normal) {

  // Check for degenerate triangle
  const Vec3 edge1 = Vec3_Subtract(v[1], v[0]);
  const Vec3 edge2 = Vec3_Subtract(v[2], v[0]);
  const Vec3 cross = Vec3_Cross(edge1, edge2);
  if (Vec3_Length(cross) < 1.f) {
    return;
  }

  if (numBrushes >= MAX_BSP_BRUSHES) {
    Com_Error(ERROR_FATAL, "MAX_BSP_BRUSHES\n");
  }
  if (numBrushSides + 5 >= MAX_BSP_BRUSH_SIDES) {
    Com_Error(ERROR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
  }

  const int32_t caulkMaterial = LoadMaterial("common/caulk");

  Brush *brush = &brushes[numBrushes];
  memset(brush, 0, sizeof(*brush));
  brush->entity = (int32_t) (entity - entities);
  brush->brush = numBrushes - entity->firstBrush;
  brush->brushSides = &brushSides[numBrushSides];

  // Front face: derive from triangle vertices so all 3 lie exactly on the plane.
  // Flip to match the outward Bézier surface normal direction.
  const bool flip = Vec3_Dot(cross, normal) < 0.f;
  const Vec3 frontNormal = flip ? Vec3_Negate(Vec3_Normalize(cross))
                                   : Vec3_Normalize(cross);
  const double frontDist = Vec3_Dot(frontNormal, v[0]);

  // Back face: opposite normal, offset by thickness
  const Vec3 backNormal = Vec3_Negate(frontNormal);
  const double backDist = -frontDist + PATCH_COLLISION_THICKNESS;

  // Side planes: for each edge, the outward normal is edge × front_normal.
  // When we flipped front_normal, we must also flip edges to keep side normals outward.
  const float ws = flip ? -1.f : 1.f;
  const Vec3 edges[3] = {
    Vec3_Scale(Vec3_Subtract(v[1], v[0]), ws),
    Vec3_Scale(Vec3_Subtract(v[2], v[1]), ws),
    Vec3_Scale(Vec3_Subtract(v[0], v[2]), ws),
  };

  int32_t numSides = 0;
  BrushSide *side;

  // Front
  side = &brush->brushSides[numSides];
  memset(side, 0, sizeof(*side));
  side->plane = FindPlane(frontNormal, frontDist);
  side->contents = CONTENTS_SOLID | CONTENTS_DETAIL;
  side->surface = SURF_NO_DRAW;
  side->material = caulkMaterial;
  q_strlcpy(side->texture, "common/caulk", sizeof(side->texture));
  side->scale = MakeVec2(1.f, 1.f);
  numSides++;

  // Back
  side = &brush->brushSides[numSides];
  memset(side, 0, sizeof(*side));
  side->plane = FindPlane(backNormal, backDist);
  side->contents = CONTENTS_SOLID | CONTENTS_DETAIL;
  side->surface = SURF_NO_DRAW;
  side->material = caulkMaterial;
  q_strlcpy(side->texture, "common/caulk", sizeof(side->texture));
  side->scale = MakeVec2(1.f, 1.f);
  numSides++;

  // 3 side planes
  for (int32_t i = 0; i < 3; i++) {
    Vec3 sideNormal = Vec3_Cross(edges[i], frontNormal);
    const float len = Vec3_Length(sideNormal);
    if (len < 0.1f) {
      continue;
    }
    sideNormal = Vec3_Scale(sideNormal, 1.f / len);
    const double sideDist = Vec3_Dot(sideNormal, v[i]);

    side = &brush->brushSides[numSides];
    memset(side, 0, sizeof(*side));
    side->plane = FindPlane(sideNormal, sideDist);
    side->contents = CONTENTS_SOLID | CONTENTS_DETAIL;
    side->surface = SURF_NO_DRAW;
    side->material = caulkMaterial;
    q_strlcpy(side->texture, "common/caulk", sizeof(side->texture));
    side->scale = MakeVec2(1.f, 1.f);
    numSides++;
  }

  if (numSides < 4) {
    return;
  }

  brush->numBrushSides = numSides;
  numBrushSides += numSides;
  numBrushes++;

  brush->contents = CONTENTS_SOLID | CONTENTS_DETAIL;

  MakeBrushWindings(brush);

  if (brush->numBrushSides) {
    AddBrushBevels(brush);
    entity->numBrushes++;
    entity->numBrushSides += brush->numBrushSides;
    entity->bounds = Box3_Union(entity->bounds, brush->bounds);
  }
}

/**
 * @brief Generates collision brushes for a patch by tessellating at a coarse
 * resolution and emitting caulk brushes for each quad cell.
 */
void EmitPatchCollisionBrushes(Patch *patch, Entity *entity) {

  if (patch->material < 0) {
    return;
  }

  const int32_t numSubPatchesS = (patch->width - 1) / 2;
  const int32_t numSubPatchesT = (patch->height - 1) / 2;

  for (int32_t subT = 0; subT < numSubPatchesT; subT++) {
    for (int32_t subS = 0; subS < numSubPatchesS; subS++) {

      // Extract the 3×3 control point sub-grid
      PatchControlPoint subCp[3][3];
      for (int32_t row = 0; row < 3; row++) {
        for (int32_t col = 0; col < 3; col++) {
          const int32_t srcRow = subT * 2 + row;
          const int32_t srcCol = subS * 2 + col;
          subCp[row][col] = patch->controlPoints[srcRow * patch->width + srcCol];
        }
      }

      // Determine subdivision count based on sub-patch size
      Vec3 corner00, corner10, corner01;
      Vec2 stDummy;
      EvaluatePatch(subCp, 0.f, 0.f, &corner00, &stDummy, NULL);
      EvaluatePatch(subCp, 1.f, 0.f, &corner10, &stDummy, NULL);
      EvaluatePatch(subCp, 0.f, 1.f, &corner01, &stDummy, NULL);

      const float sizeS = Vec3_Distance(corner00, corner10);
      const float sizeT = Vec3_Distance(corner00, corner01);

      const int32_t subdivsS = Maxi(1, (int32_t) (sizeS / PATCH_COLLISION_FACET_SIZE));
      const int32_t subdivsT = Maxi(1, (int32_t) (sizeT / PATCH_COLLISION_FACET_SIZE));

      // Tessellate and emit a brush for each quad cell
      for (int32_t j = 0; j < subdivsT; j++) {
        for (int32_t i = 0; i < subdivsS; i++) {

          const float s0 = (float) i / (float) subdivsS;
          const float s1 = (float) (i + 1) / (float) subdivsS;
          const float t0 = (float) j / (float) subdivsT;
          const float t1 = (float) (j + 1) / (float) subdivsT;

          Vec3 verts[4], normal;
          Vec2 st;
          Vec3 n00, n10, n01, n11;

          EvaluatePatch(subCp, s0, t0, &verts[0], &st, &n00);
          EvaluatePatch(subCp, s1, t0, &verts[1], &st, &n10);
          EvaluatePatch(subCp, s0, t1, &verts[2], &st, &n01);
          EvaluatePatch(subCp, s1, t1, &verts[3], &st, &n11);

          // Average normal for extrusion direction
          normal = Vec3_Normalize(Vec3_Add(Vec3_Add(n00, n10), Vec3_Add(n01, n11)));

          // Split quad into two triangles and emit a prism brush for each
          const Vec3 tri0[3] = { verts[0], verts[1], verts[2] };
          const Vec3 tri1[3] = { verts[1], verts[3], verts[2] };
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
static void EvaluatePatch(const PatchControlPoint cp[3][3],
              float s, float t,
              Vec3 *outPosition,
              Vec2 *outSt,
              Vec3 *outNormal) {

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
    outPosition->xyz[k] = BezierQuadratic(r0, r1, r2, s);
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
    outSt->xy[k] = BezierQuadratic(r0, r1, r2, s);
  }

  if (outNormal) {
    // Compute partial derivatives for normal
    Vec3 ds, dt;
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

    *outNormal = Vec3_Cross(dt, ds);
    const float len = Vec3_Length(*outNormal);
    if (len > 0.f) {
      *outNormal = Vec3_Scale(*outNormal, 1.f / len);
    } else {
      *outNormal = MakeVec3(0.f, 0.f, 1.f);
    }
  }
}

/**
 * @brief Pre-tessellates all patches belonging to the given entity.
 * @details Tessellates each 3×3 sub-patch into `PatchFace` structures stored
 * on the `Patch`, and updates the patch bounds accordingly. These precomputed
 * faces are later used when emitting BSP geometry.
 */
void TessellatePatches(int32_t entityNum) {

  const int32_t subdivisions = PATCH_SUBDIVISIONS;

  for (int32_t p = 0; p < numPatches; p++) {
    Patch *patch = &patches[p];

    if (patch->entity != entityNum) {
      continue;
    }

    if (patch->material < 0) {
      continue;
    }

    const int32_t numSubPatchesS = (patch->width - 1) / 2;
    const int32_t numSubPatchesT = (patch->height - 1) / 2;

    patch->numFaces = numSubPatchesS * numSubPatchesT;
    patch->faces = Mem_TagMalloc(patch->numFaces * sizeof(PatchFace), (MemTag) MEM_TAG_PATCH);
    patch->bounds = Box3_Null();

    int32_t faceIndex = 0;
    for (int32_t subT = 0; subT < numSubPatchesT; subT++) {
      for (int32_t subS = 0; subS < numSubPatchesS; subS++) {

        // Extract the 3×3 control point sub-grid
        PatchControlPoint subCp[3][3];
        for (int32_t row = 0; row < 3; row++) {
          for (int32_t col = 0; col < 3; col++) {
            const int32_t srcRow = subT * 2 + row;
            const int32_t srcCol = subS * 2 + col;
            subCp[row][col] = patch->controlPoints[srcRow * patch->width + srcCol];
          }
        }

        PatchFace *pf = &patch->faces[faceIndex++];
        memset(pf, 0, sizeof(*pf));
        pf->patch = patch;
        pf->bounds = Box3_Null();

        const int32_t vertsPerEdge = subdivisions + 1;

        // Tessellate: generate (subdivisions+1)² vertices
        for (int32_t j = 0; j <= subdivisions; j++) {
          const float t = (float) j / (float) subdivisions;

          for (int32_t i = 0; i <= subdivisions; i++) {
            const float s = (float) i / (float) subdivisions;

            BspVertex *v = &pf->vertexes[pf->numVertexes];
            memset(v, 0, sizeof(*v));

            Vec3 normal;
            Vec2 st;
            EvaluatePatch(subCp, s, t, &v->position, &st, &normal);

            v->normal = normal;
            v->diffusemap.x = st.x;
            v->diffusemap.y = st.y;
            v->color = MakeColor32(255, 255, 255, 255);

            pf->bounds = Box3_Append(pf->bounds, v->position);

            pf->numVertexes++;
          }
        }

        assert(pf->numVertexes == vertsPerEdge * vertsPerEdge);

        // Generate triangle elements (local 0-based indices)
        for (int32_t j = 0; j < subdivisions; j++) {
          for (int32_t i = 0; i < subdivisions; i++) {
            const int32_t base = j * vertsPerEdge + i;

            pf->elements[pf->numElements++] = base;
            pf->elements[pf->numElements++] = base + vertsPerEdge + 1;
            pf->elements[pf->numElements++] = base + vertsPerEdge;

            pf->elements[pf->numElements++] = base;
            pf->elements[pf->numElements++] = base + 1;
            pf->elements[pf->numElements++] = base + vertsPerEdge + 1;
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
static void AssignPatchFaceToNode_r(Node *node, PatchFace *pf) {

  if (node->plane == PLANE_LEAF) {
    return;
  }

  const Vec3 center = Box3_Center(pf->bounds);

  for (int32_t i = 0; i < 2; i++) {
    if (node->children[i]->plane != PLANE_LEAF &&
        Box3_ContainsPoint(node->children[i]->bounds, center)) {
      AssignPatchFaceToNode_r(node->children[i], pf);
      return;
    }
  }

  pf->next = node->patchFaces;
  node->patchFaces = pf;
}

/**
 * @brief Assigns pre-tessellated patch faces to BSP tree nodes.
 */
void AssignPatchFacesToNodes(Node *headNode, int32_t entityNum) {

  for (int32_t p = 0; p < numPatches; p++) {
    Patch *patch = &patches[p];

    if (patch->entity != entityNum) {
      continue;
    }

    for (int32_t f = 0; f < patch->numFaces; f++) {
      AssignPatchFaceToNode_r(headNode, &patch->faces[f]);
    }
  }
}

/**
 * @brief Frees pre-tessellated patch face data for the given entity.
 */
void FreePatchFaces(int32_t entityNum) {

  for (int32_t p = 0; p < numPatches; p++) {
    Patch *patch = &patches[p];

    if (patch->entity != entityNum) {
      continue;
    }

    Mem_Free(patch->faces);
    patch->faces = NULL;
    patch->numFaces = 0;
  }
}

/**
 * @brief Emits patches belonging to the given model to the BSP patches lump.
 */
void EmitPatches(const BspModel *mod) {

  const int32_t entityNum = mod->entity;

  for (int32_t p = 0; p < numPatches; p++) {
    Patch *patch = &patches[p];

    if (patch->entity != entityNum) {
      continue;
    }

    if (patch->material < 0) {
      continue;
    }

    if (bspFile.numPatches >= MAX_BSP_PATCHES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_PATCHES\n");
    }

    patch->out = &bspFile.patches[bspFile.numPatches];
    memset(patch->out, 0, sizeof(*patch->out));
    bspFile.numPatches++;

    patch->out->entity = patch->entity;
    patch->out->material = patch->material;
    patch->out->contents = patch->contents;
    patch->out->surface = patch->surface;
    patch->out->width = patch->width;
    patch->out->height = patch->height;

    // Set the patch index on all emitted BSP faces
    const int32_t patchIndex = (int32_t) (patch->out - bspFile.patches);

    for (int32_t f = 0; f < patch->numFaces; f++) {
      if (patch->faces[f].out) {
        patch->faces[f].out->patch = patchIndex;
      }
    }

    const int32_t numPoints = patch->width * patch->height;
    for (int32_t i = 0; i < numPoints; i++) {
      patch->out->controlPoints[i].position = patch->controlPoints[i].position;
      patch->out->controlPoints[i].st = patch->controlPoints[i].st;
    }
  }
}
