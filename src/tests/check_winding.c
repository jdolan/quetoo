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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "tests.h"
#include "common/winding.h"

Quetoo quetoo;

/**
 * @brief Setup fixture.
 */
void setup(void) {
  Mem_Init();
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {
  Mem_Shutdown();
}

/**
 * @brief Prints a winding's point list to stdout for debugging.
 */
static void __attribute__((unused)) PrintWinding(const char *name, const Winding *w) {

  if (w) {
    printf("%s: %p has %d points\n", name, w, w->numPoints);
    for (int32_t i = 0; i < w->numPoints; i++) {
      printf("(%08.3f, %08.3f, %08.3f)\n", w->points[i].x, w->points[i].y, w->points[i].z);
    }
  } else {
    printf("%s: NULL\n", name);
  }
}

START_TEST(check_Winding_Clip_front) {

  Winding *a = Winding_Alloc(4);
  a->numPoints = 4;

  a->points[0] = MakeVec3(0,    0,   0);
  a->points[1] = MakeVec3(1024, 0,   0);
  a->points[2] = MakeVec3(1024, 0, 512);
  a->points[3] = MakeVec3(0,    0, 512);

  Winding *b = Winding_Copy(a);

  Winding_Clip(&a, MakeVec3(1, 0, 0), -1, SIDE_EPSILON);

  ck_assert_int_eq(b->numPoints, a->numPoints);

  for (int32_t i = 0; i < a->numPoints; i++) {
    ck_assert(Vec3_Equal(b->points[i], a->points[i]));
  }

  Winding_Free(a);
  Winding_Free(b);

} END_TEST

START_TEST(check_Winding_Clip_back) {

  Winding *a = Winding_Alloc(4);
  a->numPoints = 4;

  a->points[0] = MakeVec3(0,    0,   0);
  a->points[1] = MakeVec3(1024, 0,   0);
  a->points[2] = MakeVec3(1024, 0, 512);
  a->points[3] = MakeVec3(0,    0, 512);

  Winding_Clip(&a, MakeVec3(1, 0, 0), 1024 + SIDE_EPSILON, SIDE_EPSILON);

  ck_assert_ptr_eq(NULL, a);

} END_TEST

START_TEST(check_Winding_Clip_both) {

  Winding *a = Winding_Alloc(4);
  a->numPoints = 4;

  a->points[0] = MakeVec3(0,    0,   0);
  a->points[1] = MakeVec3(1024, 0,   0);
  a->points[2] = MakeVec3(1024, 0, 512);
  a->points[3] = MakeVec3(0,    0, 512);

  Winding *b = Winding_Copy(a);

  b->points[0] = MakeVec3(512,  0,   0);
  b->points[3] = MakeVec3(512,  0, 512);

  Winding_Clip(&a, MakeVec3(1, 0, 0), 512, SIDE_EPSILON);

  ck_assert_int_eq(b->numPoints, a->numPoints);

  for (int32_t i = 0; i < a->numPoints; i++) {
    ck_assert(Vec3_Equal(b->points[i], a->points[i]));
  }

  Winding_Free(a);
  Winding_Free(b);

} END_TEST

START_TEST(check_Winding_Clip_on) {

  Winding *a = Winding_Alloc(4);
  a->numPoints = 4;

  a->points[0] = MakeVec3(0,    0,   0);
  a->points[1] = MakeVec3(1024, 0,   0);
  a->points[2] = MakeVec3(1024, 0, 512);
  a->points[3] = MakeVec3(0,    0, 512);

  Winding *b = Winding_Copy(a);

  Winding_Clip(&a, MakeVec3(1, 0, 0), 0, SIDE_EPSILON);

  ck_assert_int_eq(b->numPoints, a->numPoints);

  for (int32_t i = 0; i < a->numPoints; i++) {
    ck_assert(Vec3_Equal(b->points[i], a->points[i]));
  }

  Winding_Clip(&a, MakeVec3(-1, 0, 0), -1024, SIDE_EPSILON);

  ck_assert_int_eq(b->numPoints, a->numPoints);

  for (int32_t i = 0; i < a->numPoints; i++) {
    ck_assert(Vec3_Equal(b->points[i], a->points[i]));
  }

  Winding_Free(a);
  Winding_Free(b);

} END_TEST

START_TEST(check_Winding_Elements_triangle) {

  Winding *w = Winding_Alloc(3);
  w->numPoints = 3;

  w->points[0] = MakeVec3(0, 0, 0);
  w->points[1] = MakeVec3(1, 0, 0);
  w->points[2] = MakeVec3(0, 1, 0);

  int32_t elements[(w->numPoints - 2) * 3];
  const int32_t numElements = Winding_Elements(w, elements);

  ck_assert_int_eq(3, numElements);

  ck_assert_int_eq(0, elements[0]);
  ck_assert_int_eq(1, elements[1]);
  ck_assert_int_eq(2, elements[2]);

} END_TEST

START_TEST(check_Winding_Elements_quad) {

  Winding *w = Winding_Alloc(4);
  w->numPoints = 4;

  w->points[0] = MakeVec3(0, 0, 0);
  w->points[1] = MakeVec3(1, 0, 0);
  w->points[2] = MakeVec3(1, 1, 0);
  w->points[3] = MakeVec3(0, 1, 0);

  int32_t elements[(w->numPoints - 2) * 3];
  const int32_t numElements = Winding_Elements(w, elements);

  ck_assert_int_eq(6, numElements);

  ck_assert_int_eq(0, elements[0]);
  ck_assert_int_eq(1, elements[1]);
  ck_assert_int_eq(2, elements[2]);

  ck_assert_int_eq(0, elements[3]);
  ck_assert_int_eq(2, elements[4]);
  ck_assert_int_eq(3, elements[5]);

} END_TEST

START_TEST(check_Winding_Elements_skinnyQuad) {

  Winding *w = Winding_Alloc(4);
  w->numPoints = 4;

  w->points[0] = MakeVec3(0, 0, 0);
  w->points[1] = MakeVec3(128, 0, 0);
  w->points[2] = MakeVec3(128, 1, 0);
  w->points[3] = MakeVec3(0, 1, 0);

  int32_t elements[(w->numPoints - 2) * 3];
  const int32_t numElements = Winding_Elements(w, elements);

  ck_assert_int_eq(6, numElements);

  ck_assert_int_eq(0, elements[0]);
  ck_assert_int_eq(1, elements[1]);
  ck_assert_int_eq(2, elements[2]);

  ck_assert_int_eq(0, elements[3]);
  ck_assert_int_eq(2, elements[4]);
  ck_assert_int_eq(3, elements[5]);

} END_TEST

START_TEST(check_Winding_Elements_colinearQuad) {

  Winding *w = Winding_Alloc(6);
  w->numPoints = 6;

  w->points[0] = MakeVec3(0, 0, 0);
  w->points[1] = MakeVec3(1, 0, 0);
  w->points[2] = MakeVec3(2, 0, 0);

  w->points[3] = MakeVec3(2, 2, 0);
  w->points[4] = MakeVec3(1, 2, 0);
  w->points[5] = MakeVec3(0, 2, 0);

  int32_t elements[(w->numPoints - 2) * 3];
  const int32_t numElements = Winding_Elements(w, elements);

  ck_assert_int_eq(12, numElements);

  ck_assert_int_eq(1, elements[0]);
  ck_assert_int_eq(2, elements[1]);
  ck_assert_int_eq(3, elements[2]);

  ck_assert_int_eq(4, elements[3]);
  ck_assert_int_eq(5, elements[4]);
  ck_assert_int_eq(0, elements[5]);

  ck_assert_int_eq(0, elements[6]);
  ck_assert_int_eq(1, elements[7]);
  ck_assert_int_eq(3, elements[8]);

  ck_assert_int_eq(0, elements[9]);
  ck_assert_int_eq(3, elements[10]);
  ck_assert_int_eq(4, elements[11]);

} END_TEST

START_TEST(check_Winding_Elements_cornerCase) {

  Winding *w = Winding_Alloc(6);
  w->numPoints = 6;

  w->points[0] = MakeVec3(0, 0.000, 0.000);
  w->points[1] = MakeVec3(0, -1.375, 2.000);
  w->points[2] = MakeVec3(0, -20.875, 31.375);
  w->points[3] = MakeVec3(0, -30.750, 30.750);
  w->points[4] = MakeVec3(0, -19.500, 19.500);
  w->points[5] = MakeVec3(0, -12.000, 12.000);

  int32_t elements[(w->numPoints - 2) * 3];
  const int32_t numElements = Winding_Elements(w, elements);

  ck_assert_int_eq(12, numElements);

} END_TEST

START_TEST(check_Winding_Elements_invalid) {

  Winding *w = Winding_Alloc(3);
  w->numPoints = 3;

  // This is a real invalid winding emitted from edge.map. Plotting this winding shows
  // that indeed, the points are essentially colinear. This is a better test than the
  // somewhat contrived ones above.

  w->points[2] = MakeVec3(1516.32446, 1435.71924, 592.605286);
  w->points[1] = MakeVec3(1511.57983, 1428.12781, 595.452087);
  w->points[0] = MakeVec3(1506.83521, 1420.53638, 598.298889);

  int32_t elements[(w->numPoints - 2) * 3];
  const int32_t numElements = Winding_Elements(w, elements);

  ck_assert_int_eq(0, numElements);

} END_TEST

START_TEST(check_Vec3_TriangleArea) {
  Vec3 a, b, c;

  a = MakeVec3(0, 0, 0);
  b = MakeVec3(0, 1, 0);
  c = MakeVec3(1, 1, 0);

  const float area = Vec3_TriangleArea(a, b, c);
  ck_assert(area == 0.5);

} END_TEST

START_TEST(check_Vec3_Barycentric) {
  Vec3 a, b, c, p, out;

  a = MakeVec3(0, 0, 0);
  b = MakeVec3(0, 1, 0);
  c = MakeVec3(1, 1, 0);

  p = MakeVec3(0, 0, 0);

  Vec3_Barycentric(a, b, c, p, &out);
//  puts(vtos(out));
  ck_assert(out.x == 1);
  ck_assert(out.y == 0);
  ck_assert(out.z == 0);

  p = MakeVec3(0, 1, 0);

  Vec3_Barycentric(a, b, c, p, &out);
//  puts(vtos(out));
  ck_assert(out.x == 0);
  ck_assert(out.y == 1);
  ck_assert(out.z == 0);

  p = MakeVec3(1, 1, 0);

  Vec3_Barycentric(a, b, c, p, &out);
//  puts(vtos(out));
  ck_assert(out.x == 0);
  ck_assert(out.y == 0);
  ck_assert(out.z == 1);

  p = MakeVec3(0.5, 0.5, 0);

  Vec3_Barycentric(a, b, c, p, &out);
//  puts(vtos(out));
  ck_assert(out.x == 0.5);
  ck_assert(out.y == 0);
  ck_assert(out.z == 0.5);

} END_TEST

START_TEST(check_Winding_ClipToWinding_full_inside) {
  // Clip a small quad completely inside a larger quad
  Winding *large = Winding_Alloc(4);
  large->numPoints = 4;
  large->points[0] = MakeVec3(0, 0, 0);
  large->points[1] = MakeVec3(100, 0, 0);
  large->points[2] = MakeVec3(100, 0, 100);
  large->points[3] = MakeVec3(0, 0, 100);

  Winding *small = Winding_Alloc(4);
  small->numPoints = 4;
  small->points[0] = MakeVec3(25, 0, 25);
  small->points[1] = MakeVec3(75, 0, 25);
  small->points[2] = MakeVec3(75, 0, 75);
  small->points[3] = MakeVec3(25, 0, 75);

  Winding *result = Winding_ClipToWinding(small, large, MakeVec3(0, 1, 0), SIDE_EPSILON);

  ck_assert_ptr_nonnull(result);
  ck_assert_int_eq(4, result->numPoints);
  
  // Should be unchanged since it's fully inside
  for (int32_t i = 0; i < 4; i++) {
    ck_assert(Vec3_EqualEpsilon(small->points[i], result->points[i], 0.01f));
  }

  Winding_Free(large);
  Winding_Free(small);
  Winding_Free(result);

} END_TEST

START_TEST(check_Winding_ClipToWinding_full_outside) {
  // Clip a quad completely outside another quad
  Winding *clip = Winding_Alloc(4);
  clip->numPoints = 4;
  clip->points[0] = MakeVec3(0, 0, 0);
  clip->points[1] = MakeVec3(50, 0, 0);
  clip->points[2] = MakeVec3(50, 0, 50);
  clip->points[3] = MakeVec3(0, 0, 50);

  Winding *outside = Winding_Alloc(4);
  outside->numPoints = 4;
  outside->points[0] = MakeVec3(100, 0, 100);
  outside->points[1] = MakeVec3(150, 0, 100);
  outside->points[2] = MakeVec3(150, 0, 150);
  outside->points[3] = MakeVec3(100, 0, 150);

  Winding *result = Winding_ClipToWinding(outside, clip, MakeVec3(0, 1, 0), SIDE_EPSILON);

  ck_assert_ptr_null(result);

  Winding_Free(clip);
  Winding_Free(outside);

} END_TEST

/**
 * @brief Builds an axis-aligned quad in the y = 0 plane.
 */
static Winding *CheckQuad(float x0, float z0, float x1, float z1) {

  Winding *w = Winding_Alloc(4);
  w->numPoints = 4;
  w->points[0] = MakeVec3(x0, 0, z0);
  w->points[1] = MakeVec3(x1, 0, z0);
  w->points[2] = MakeVec3(x1, 0, z1);
  w->points[3] = MakeVec3(x0, 0, z1);

  return w;
}

START_TEST(check_Winding_ClipToWindingInto_parity) {
  // The allocation-free variant must agree with the allocating one
  const float cases[][4] = {
    { 25.f, 25.f, 75.f,  75.f  }, // partial overlap
    { 10.f, 10.f, 40.f,  40.f  }, // fully inside
    { -5.f, -5.f, 55.f,  55.f  }, // fully surrounding
    {  0.f,  0.f, 50.f,  50.f  }, // edge aligned
    { 25.f, -5.f, 75.f,  25.f  }, // corner overlap
  };

  Winding *clip = CheckQuad(0.f, 0.f, 50.f, 50.f);

  for (size_t c = 0; c < lengthof(cases); c++) {
    Winding *in = CheckQuad(cases[c][0], cases[c][1], cases[c][2], cases[c][3]);

    Winding *expected = Winding_ClipToWinding(in, clip, MakeVec3(0, 1, 0), SIDE_EPSILON);

    const int32_t capacity = in->numPoints + 4 * clip->numPoints;
    Winding *a = Winding_Alloc(capacity);
    Winding *b = Winding_Alloc(capacity);

    const Winding *actual = Winding_ClipToWindingInto(in, clip, MakeVec3(0, 1, 0),
                                                             SIDE_EPSILON, a, b, capacity);

    if (expected == NULL) {
      ck_assert_ptr_null(actual);
    } else {
      ck_assert_ptr_nonnull(actual);
      ck_assert_int_eq(actual->numPoints, expected->numPoints);
      for (int32_t i = 0; i < expected->numPoints; i++) {
        ck_assert_float_eq_tol(actual->points[i].x, expected->points[i].x, SIDE_EPSILON);
        ck_assert_float_eq_tol(actual->points[i].y, expected->points[i].y, SIDE_EPSILON);
        ck_assert_float_eq_tol(actual->points[i].z, expected->points[i].z, SIDE_EPSILON);
      }
      Winding_Free(expected);
    }

    Winding_Free(a);
    Winding_Free(b);
    Winding_Free(in);
  }

  Winding_Free(clip);

} END_TEST

START_TEST(check_Winding_ClipToWindingInto_full_outside) {
  Winding *clip = CheckQuad(0.f, 0.f, 50.f, 50.f);
  Winding *in = CheckQuad(100.f, 100.f, 150.f, 150.f);

  const int32_t capacity = in->numPoints + 4 * clip->numPoints;
  Winding *a = Winding_Alloc(capacity);
  Winding *b = Winding_Alloc(capacity);

  ck_assert_ptr_null(Winding_ClipToWindingInto(in, clip, MakeVec3(0, 1, 0), SIDE_EPSILON,
                                                 a, b, capacity));

  Winding_Free(a);
  Winding_Free(b);
  Winding_Free(in);
  Winding_Free(clip);

} END_TEST

START_TEST(check_Winding_ClipToWindingInto_full_inside) {
  // Nothing clips, so the input itself is returned and the scratch is untouched
  Winding *clip = CheckQuad(0.f, 0.f, 50.f, 50.f);
  Winding *in = CheckQuad(10.f, 10.f, 40.f, 40.f);

  const int32_t capacity = in->numPoints + 4 * clip->numPoints;
  Winding *a = Winding_Alloc(capacity);
  Winding *b = Winding_Alloc(capacity);

  const Winding *result = Winding_ClipToWindingInto(in, clip, MakeVec3(0, 1, 0),
                                                           SIDE_EPSILON, a, b, capacity);

  ck_assert_ptr_eq(result, in);

  Winding_Free(a);
  Winding_Free(b);
  Winding_Free(in);
  Winding_Free(clip);

} END_TEST

START_TEST(check_Winding_ClipToWinding_partial_overlap) {
  // Clip a quad partially overlapping another
  Winding *clip = Winding_Alloc(4);
  clip->numPoints = 4;
  clip->points[0] = MakeVec3(0, 0, 0);
  clip->points[1] = MakeVec3(50, 0, 0);
  clip->points[2] = MakeVec3(50, 0, 50);
  clip->points[3] = MakeVec3(0, 0, 50);

  Winding *overlap = Winding_Alloc(4);
  overlap->numPoints = 4;
  overlap->points[0] = MakeVec3(25, 0, 25);
  overlap->points[1] = MakeVec3(75, 0, 25);
  overlap->points[2] = MakeVec3(75, 0, 75);
  overlap->points[3] = MakeVec3(25, 0, 75);

  Winding *result = Winding_ClipToWinding(overlap, clip, MakeVec3(0, 1, 0), SIDE_EPSILON);

  ck_assert_ptr_nonnull(result);
  ck_assert_int_ge(result->numPoints, 3); // At least a triangle
  
  // Verify all result points are within the clip bounds
  for (int32_t i = 0; i < result->numPoints; i++) {
    ck_assert_float_ge(result->points[i].x, -SIDE_EPSILON);
    ck_assert_float_le(result->points[i].x, 50.f + SIDE_EPSILON);
    ck_assert_float_ge(result->points[i].z, -SIDE_EPSILON);
    ck_assert_float_le(result->points[i].z, 50.f + SIDE_EPSILON);
  }

  Winding_Free(clip);
  Winding_Free(overlap);
  Winding_Free(result);

} END_TEST

START_TEST(check_Winding_ClipToWinding_triangle) {
  // Clip a quad to a triangular region
  Winding *triangle = Winding_Alloc(3);
  triangle->numPoints = 3;
  triangle->points[0] = MakeVec3(0, 0, 0);
  triangle->points[1] = MakeVec3(100, 0, 0);
  triangle->points[2] = MakeVec3(50, 0, 100);

  Winding *quad = Winding_Alloc(4);
  quad->numPoints = 4;
  quad->points[0] = MakeVec3(10, 0, 10);
  quad->points[1] = MakeVec3(90, 0, 10);
  quad->points[2] = MakeVec3(90, 0, 90);
  quad->points[3] = MakeVec3(10, 0, 90);

  Winding *result = Winding_ClipToWinding(quad, triangle, MakeVec3(0, 1, 0), SIDE_EPSILON);

  ck_assert_ptr_nonnull(result);
  ck_assert_int_ge(result->numPoints, 3); // At least a triangle

  Winding_Free(triangle);
  Winding_Free(quad);
  Winding_Free(result);

} END_TEST

START_TEST(check_Winding_ClipToWinding_vertical_plane) {
  // Test clipping on a vertical plane (wall)
  Winding *clip = Winding_Alloc(4);
  clip->numPoints = 4;
  clip->points[0] = MakeVec3(0, 0, 0);
  clip->points[1] = MakeVec3(0, 0, 100);
  clip->points[2] = MakeVec3(0, 100, 100);
  clip->points[3] = MakeVec3(0, 100, 0);
  
  Winding *in = Winding_Alloc(4);
  in->numPoints = 4;
  in->points[0] = MakeVec3(0, 25, 25);
  in->points[1] = MakeVec3(0, 25, 75);
  in->points[2] = MakeVec3(0, 75, 75);
  in->points[3] = MakeVec3(0, 75, 25);
  
  Winding *result = Winding_ClipToWinding(in, clip, MakeVec3(1, 0, 0), SIDE_EPSILON);
  
  ck_assert_ptr_nonnull(result);
  ck_assert_int_eq(4, result->numPoints);
  
  // Result should be the same as input (fully inside)
  for (int32_t i = 0; i < 4; i++) {
    ck_assert(Vec3_EqualEpsilon(in->points[i], result->points[i], 0.01f));
  }
  
  Winding_Free(clip);
  Winding_Free(in);
  Winding_Free(result);

} END_TEST

START_TEST(check_Winding_ClipToWinding_diagonal_plane) {
  // Test clipping on a diagonal plane
  const Vec3 normal = Vec3_Normalize(MakeVec3(1, 0, 1));
  
  // Create a square on the diagonal plane
  const Vec3 tangent = Vec3_Normalize(MakeVec3(-1, 0, 1));
  const Vec3 bitangent = MakeVec3(0, 1, 0);
  
  Winding *clip = Winding_Alloc(4);
  clip->numPoints = 4;
  clip->points[0] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, -50), Vec3_Scale(bitangent, -50)), Vec3_Zero());
  clip->points[1] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, 50), Vec3_Scale(bitangent, -50)), Vec3_Zero());
  clip->points[2] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, 50), Vec3_Scale(bitangent, 50)), Vec3_Zero());
  clip->points[3] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, -50), Vec3_Scale(bitangent, 50)), Vec3_Zero());
  
  // Small square in the center
  Winding *in = Winding_Alloc(4);
  in->numPoints = 4;
  in->points[0] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, -10), Vec3_Scale(bitangent, -10)), Vec3_Zero());
  in->points[1] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, 10), Vec3_Scale(bitangent, -10)), Vec3_Zero());
  in->points[2] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, 10), Vec3_Scale(bitangent, 10)), Vec3_Zero());
  in->points[3] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, -10), Vec3_Scale(bitangent, 10)), Vec3_Zero());
  
  Winding *result = Winding_ClipToWinding(in, clip, normal, SIDE_EPSILON);
  
  ck_assert_ptr_nonnull(result);
  ck_assert_int_eq(4, result->numPoints);
  
  Winding_Free(clip);
  Winding_Free(in);
  Winding_Free(result);

} END_TEST

START_TEST(check_Winding_ClipToWinding_offset_planes) {
  // Test clipping windings that are offset from each other
  Winding *clip = Winding_Alloc(4);
  clip->numPoints = 4;
  clip->points[0] = MakeVec3(0, 1, 0);
  clip->points[1] = MakeVec3(100, 1, 0);
  clip->points[2] = MakeVec3(100, 1, 100);
  clip->points[3] = MakeVec3(0, 1, 100);
  
  // Decal winding offset by 1 unit in normal direction
  Winding *in = Winding_Alloc(4);
  in->numPoints = 4;
  in->points[0] = MakeVec3(25, 2, 25);
  in->points[1] = MakeVec3(75, 2, 25);
  in->points[2] = MakeVec3(75, 2, 75);
  in->points[3] = MakeVec3(25, 2, 75);
  
  // This should still work if we use a large enough epsilon
  Winding *result = Winding_ClipToWinding(in, clip, MakeVec3(0, 1, 0), 2.0);
  
  ck_assert_ptr_nonnull(result);
  ck_assert_int_eq(4, result->numPoints);
  
  Winding_Free(clip);
  Winding_Free(in);
  Winding_Free(result);

} END_TEST

START_TEST(check_Winding_ClipToWinding_decal_scenario) {
  // Simulate actual decal clipping scenario
  // Face on a floor
  Winding *face = Winding_Alloc(4);
  face->numPoints = 4;
  face->points[0] = MakeVec3(-64, 0, -64);
  face->points[1] = MakeVec3(64, 0, -64);
  face->points[2] = MakeVec3(64, 0, 64);
  face->points[3] = MakeVec3(-64, 0, 64);
  
  // Decal quad in center of face
  Winding *decal = Winding_Alloc(4);
  decal->numPoints = 4;
  decal->points[0] = MakeVec3(-16, 0, -16);
  decal->points[1] = MakeVec3(16, 0, -16);
  decal->points[2] = MakeVec3(16, 0, 16);
  decal->points[3] = MakeVec3(-16, 0, 16);
  
  Winding *result = Winding_ClipToWinding(decal, face, MakeVec3(0, 1, 0), SIDE_EPSILON);
  
  ck_assert_ptr_nonnull(result);
  ck_assert_int_eq(4, result->numPoints);
  
  // Should be unchanged since decal is fully inside face
  for (int32_t i = 0; i < 4; i++) {
    ck_assert(Vec3_EqualEpsilon(decal->points[i], result->points[i], 0.01f));
  }
  
  Winding_Free(face);
  Winding_Free(decal);
  Winding_Free(result);

} END_TEST

START_TEST(check_Winding_ClipToWinding_edge_aligned) {
  // Test when decal edge aligns with face edge
  Winding *face = Winding_Alloc(4);
  face->numPoints = 4;
  face->points[0] = MakeVec3(0, 0, 0);
  face->points[1] = MakeVec3(64, 0, 0);
  face->points[2] = MakeVec3(64, 0, 64);
  face->points[3] = MakeVec3(0, 0, 64);
  
  // Decal that extends to face edge
  Winding *decal = Winding_Alloc(4);
  decal->numPoints = 4;
  decal->points[0] = MakeVec3(0, 0, 16);
  decal->points[1] = MakeVec3(32, 0, 16);
  decal->points[2] = MakeVec3(32, 0, 48);
  decal->points[3] = MakeVec3(0, 0, 48);
  
  Winding *result = Winding_ClipToWinding(decal, face, MakeVec3(0, 1, 0), SIDE_EPSILON);
  
  ck_assert_ptr_nonnull(result);
  ck_assert_int_eq(4, result->numPoints);
  
  Winding_Free(face);
  Winding_Free(decal);
  Winding_Free(result);

} END_TEST

START_TEST(check_Winding_Distance) {

  Winding *w = Winding_Alloc(3);
  w->numPoints = 3;

  w->points[0] = MakeVec3(0.f, 0.f, 0.f);
  w->points[1] = MakeVec3(0.f, 1.f, 0.f);
  w->points[2] = MakeVec3(1.f, 0.f, 0.f);

  Vec3 dir;
  ck_assert_float_eq(0.f, Winding_Distance(w, w->points[0], &dir));
  ck_assert_float_eq(0.f, Winding_Distance(w, w->points[1], &dir));
  ck_assert_float_eq(0.f, Winding_Distance(w, w->points[2], &dir));

  ck_assert_float_eq(0.f, Winding_Distance(w, MakeVec3(.5f, .5f, 0.f), &dir));
  ck_assert_float_eq(1.f, Winding_Distance(w, MakeVec3(0.f, 2.f, 0.f), &dir));

  Winding_Free(w);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  Suite *suite = suite_create("check_winding");

  {
    TCase *tcase = tcase_create("Winding_Clip");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Winding_Clip_front);
    tcase_add_test(tcase, check_Winding_Clip_back);
    tcase_add_test(tcase, check_Winding_Clip_both);
    tcase_add_test(tcase, check_Winding_Clip_on);
    suite_add_tcase(suite, tcase);
  }

  {
    TCase *tcase = tcase_create("Winding_Elements");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Winding_Elements_triangle);
    tcase_add_test(tcase, check_Winding_Elements_quad);
    tcase_add_test(tcase, check_Winding_Elements_skinnyQuad);
    tcase_add_test(tcase, check_Winding_Elements_colinearQuad);
    tcase_add_test(tcase, check_Winding_Elements_cornerCase);
    tcase_add_test(tcase, check_Winding_Elements_invalid);
    suite_add_tcase(suite, tcase);
  }

  {
    TCase *tcase = tcase_create("Vec3_TriangleArea");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Vec3_TriangleArea);
    suite_add_tcase(suite, tcase);
  }

  {
    TCase *tcase = tcase_create("Vec3_Barycentric");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Vec3_Barycentric);
    suite_add_tcase(suite, tcase);
  }

  {
    TCase *tcase = tcase_create("Winding_ClipToWinding");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Winding_ClipToWinding_full_inside);
    tcase_add_test(tcase, check_Winding_ClipToWinding_full_outside);
    tcase_add_test(tcase, check_Winding_ClipToWinding_partial_overlap);
    tcase_add_test(tcase, check_Winding_ClipToWinding_triangle);
    tcase_add_test(tcase, check_Winding_ClipToWinding_vertical_plane);
    tcase_add_test(tcase, check_Winding_ClipToWinding_diagonal_plane);
    tcase_add_test(tcase, check_Winding_ClipToWinding_offset_planes);
    tcase_add_test(tcase, check_Winding_ClipToWinding_decal_scenario);
    tcase_add_test(tcase, check_Winding_ClipToWinding_edge_aligned);
    suite_add_tcase(suite, tcase);
  }

  {
    TCase *tcase = tcase_create("Winding_ClipToWindingInto");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Winding_ClipToWindingInto_parity);
    tcase_add_test(tcase, check_Winding_ClipToWindingInto_full_outside);
    tcase_add_test(tcase, check_Winding_ClipToWindingInto_full_inside);
    suite_add_tcase(suite, tcase);
  }

  {
    TCase *tcase = tcase_create("Winding_Distance");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Winding_Distance);
    suite_add_tcase(suite, tcase);
  }

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
