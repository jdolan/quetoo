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

#include "tests.h"
#include "collision/cm_polylib.h"

quetoo_t quetoo;

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
 * @brief
 */
static void __attribute__((unused)) PrintWinding(const char *name, const cm_winding_t *w) {

  if (w) {
    printf("%s: %p has %d points\n", name, w, w->num_points);
    for (int32_t i = 0; i < w->num_points; i++) {
      printf("(%08.3f, %08.3f, %08.3f)\n", w->points[i].x, w->points[i].y, w->points[i].z);
    }
  } else {
    printf("%s: NULL\n", name);
  }
}

START_TEST(check_Cm_ClipWinding_front) {

  cm_winding_t *a = Cm_AllocWinding(4);
  a->num_points = 4;

  a->points[0] = Vec3(0,    0,   0);
  a->points[1] = Vec3(1024, 0,   0);
  a->points[2] = Vec3(1024, 0, 512);
  a->points[3] = Vec3(0,    0, 512);

  cm_winding_t *b = Cm_CopyWinding(a);

  Cm_ClipWinding(&a, Vec3(1, 0, 0), -1, SIDE_EPSILON);

  ck_assert_int_eq(b->num_points, a->num_points);

  for (int32_t i = 0; i < a->num_points; i++) {
    ck_assert(Vec3_Equal(b->points[i], a->points[i]));
  }

  Cm_FreeWinding(a);
  Cm_FreeWinding(b);

} END_TEST

START_TEST(check_Cm_ClipWinding_back) {

  cm_winding_t *a = Cm_AllocWinding(4);
  a->num_points = 4;

  a->points[0] = Vec3(0,    0,   0);
  a->points[1] = Vec3(1024, 0,   0);
  a->points[2] = Vec3(1024, 0, 512);
  a->points[3] = Vec3(0,    0, 512);

  Cm_ClipWinding(&a, Vec3(1, 0, 0), 1024 + SIDE_EPSILON, SIDE_EPSILON);

  ck_assert_ptr_eq(NULL, a);

} END_TEST

START_TEST(check_Cm_ClipWinding_both) {

  cm_winding_t *a = Cm_AllocWinding(4);
  a->num_points = 4;

  a->points[0] = Vec3(0,    0,   0);
  a->points[1] = Vec3(1024, 0,   0);
  a->points[2] = Vec3(1024, 0, 512);
  a->points[3] = Vec3(0,    0, 512);

  cm_winding_t *b = Cm_CopyWinding(a);

  b->points[0] = Vec3(512,  0,   0);
  b->points[3] = Vec3(512,  0, 512);

  Cm_ClipWinding(&a, Vec3(1, 0, 0), 512, SIDE_EPSILON);

  ck_assert_int_eq(b->num_points, a->num_points);

  for (int32_t i = 0; i < a->num_points; i++) {
    ck_assert(Vec3_Equal(b->points[i], a->points[i]));
  }

  Cm_FreeWinding(a);
  Cm_FreeWinding(b);

} END_TEST

START_TEST(check_Cm_ClipWinding_on) {

  cm_winding_t *a = Cm_AllocWinding(4);
  a->num_points = 4;

  a->points[0] = Vec3(0,    0,   0);
  a->points[1] = Vec3(1024, 0,   0);
  a->points[2] = Vec3(1024, 0, 512);
  a->points[3] = Vec3(0,    0, 512);

  cm_winding_t *b = Cm_CopyWinding(a);

  Cm_ClipWinding(&a, Vec3(1, 0, 0), 0, SIDE_EPSILON);

  ck_assert_int_eq(b->num_points, a->num_points);

  for (int32_t i = 0; i < a->num_points; i++) {
    ck_assert(Vec3_Equal(b->points[i], a->points[i]));
  }

  Cm_ClipWinding(&a, Vec3(-1, 0, 0), -1024, SIDE_EPSILON);

  ck_assert_int_eq(b->num_points, a->num_points);

  for (int32_t i = 0; i < a->num_points; i++) {
    ck_assert(Vec3_Equal(b->points[i], a->points[i]));
  }

  Cm_FreeWinding(a);
  Cm_FreeWinding(b);

} END_TEST

START_TEST(check_Cm_ElementsForWinding_triangle) {

  cm_winding_t *w = Cm_AllocWinding(3);
  w->num_points = 3;

  w->points[0] = Vec3(0, 0, 0);
  w->points[1] = Vec3(1, 0, 0);
  w->points[2] = Vec3(0, 1, 0);

  int32_t elements[(w->num_points - 2) * 3];
  const int32_t num_elements = Cm_ElementsForWinding(w, elements);

  ck_assert_int_eq(3, num_elements);

  ck_assert_int_eq(0, elements[0]);
  ck_assert_int_eq(1, elements[1]);
  ck_assert_int_eq(2, elements[2]);

} END_TEST

START_TEST(check_Cm_ElementsForWinding_quad) {

  cm_winding_t *w = Cm_AllocWinding(4);
  w->num_points = 4;

  w->points[0] = Vec3(0, 0, 0);
  w->points[1] = Vec3(1, 0, 0);
  w->points[2] = Vec3(1, 1, 0);
  w->points[3] = Vec3(0, 1, 0);

  int32_t elements[(w->num_points - 2) * 3];
  const int32_t num_elements = Cm_ElementsForWinding(w, elements);

  ck_assert_int_eq(6, num_elements);

  ck_assert_int_eq(0, elements[0]);
  ck_assert_int_eq(1, elements[1]);
  ck_assert_int_eq(2, elements[2]);

  ck_assert_int_eq(0, elements[3]);
  ck_assert_int_eq(2, elements[4]);
  ck_assert_int_eq(3, elements[5]);

} END_TEST

START_TEST(check_Cm_ElementsForWinding_skinnyQuad) {

  cm_winding_t *w = Cm_AllocWinding(4);
  w->num_points = 4;

  w->points[0] = Vec3(0, 0, 0);
  w->points[1] = Vec3(128, 0, 0);
  w->points[2] = Vec3(128, 1, 0);
  w->points[3] = Vec3(0, 1, 0);

  int32_t elements[(w->num_points - 2) * 3];
  const int32_t num_elements = Cm_ElementsForWinding(w, elements);

  ck_assert_int_eq(6, num_elements);

  ck_assert_int_eq(0, elements[0]);
  ck_assert_int_eq(1, elements[1]);
  ck_assert_int_eq(2, elements[2]);

  ck_assert_int_eq(0, elements[3]);
  ck_assert_int_eq(2, elements[4]);
  ck_assert_int_eq(3, elements[5]);

} END_TEST

START_TEST(check_Cm_ElementsForWinding_colinearQuad) {

  cm_winding_t *w = Cm_AllocWinding(6);
  w->num_points = 6;

  w->points[0] = Vec3(0, 0, 0);
  w->points[1] = Vec3(1, 0, 0);
  w->points[2] = Vec3(2, 0, 0);

  w->points[3] = Vec3(2, 2, 0);
  w->points[4] = Vec3(1, 2, 0);
  w->points[5] = Vec3(0, 2, 0);

  int32_t elements[(w->num_points - 2) * 3];
  const int32_t num_elements = Cm_ElementsForWinding(w, elements);

  ck_assert_int_eq(12, num_elements);

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

START_TEST(check_Cm_ElementsForWinding_cornerCase) {

  cm_winding_t *w = Cm_AllocWinding(6);
  w->num_points = 6;

  w->points[0] = Vec3(0, 0.000, 0.000);
  w->points[1] = Vec3(0, -1.375, 2.000);
  w->points[2] = Vec3(0, -20.875, 31.375);
  w->points[3] = Vec3(0, -30.750, 30.750);
  w->points[4] = Vec3(0, -19.500, 19.500);
  w->points[5] = Vec3(0, -12.000, 12.000);

  int32_t elements[(w->num_points - 2) * 3];
  const int32_t num_elements = Cm_ElementsForWinding(w, elements);

  ck_assert_int_eq(12, num_elements);

} END_TEST

START_TEST(check_Cm_ElementsForWinding_invalid) {

  cm_winding_t *w = Cm_AllocWinding(3);
  w->num_points = 3;

  // This is a real invalid winding emitted from edge.map. Plotting this winding shows
  // that indeed, the points are essentially colinear. This is a better test than the
  // somewhat contrived ones above.

  w->points[2] = Vec3(1516.32446, 1435.71924, 592.605286);
  w->points[1] = Vec3(1511.57983, 1428.12781, 595.452087);
  w->points[0] = Vec3(1506.83521, 1420.53638, 598.298889);

  int32_t elements[(w->num_points - 2) * 3];
  const int32_t num_elements = Cm_ElementsForWinding(w, elements);

  ck_assert_int_eq(0, num_elements);

} END_TEST

START_TEST(check_Cm_TriangleArea) {
  vec3_t a, b, c;

  a = Vec3(0, 0, 0);
  b = Vec3(0, 1, 0);
  c = Vec3(1, 1, 0);

  const float area = Cm_TriangleArea(a, b, c);
  ck_assert(area == 0.5);

} END_TEST

START_TEST(check_Cm_Barycentric) {
  vec3_t a, b, c, p, out;

  a = Vec3(0, 0, 0);
  b = Vec3(0, 1, 0);
  c = Vec3(1, 1, 0);

  p = Vec3(0, 0, 0);

  Cm_Barycentric(a, b, c, p, &out);
//  puts(vtos(out));
  ck_assert(out.x == 1);
  ck_assert(out.y == 0);
  ck_assert(out.z == 0);

  p = Vec3(0, 1, 0);

  Cm_Barycentric(a, b, c, p, &out);
//  puts(vtos(out));
  ck_assert(out.x == 0);
  ck_assert(out.y == 1);
  ck_assert(out.z == 0);

  p = Vec3(1, 1, 0);

  Cm_Barycentric(a, b, c, p, &out);
//  puts(vtos(out));
  ck_assert(out.x == 0);
  ck_assert(out.y == 0);
  ck_assert(out.z == 1);

  p = Vec3(0.5, 0.5, 0);

  Cm_Barycentric(a, b, c, p, &out);
//  puts(vtos(out));
  ck_assert(out.x == 0.5);
  ck_assert(out.y == 0);
  ck_assert(out.z == 0.5);

} END_TEST

START_TEST(check_Cm_ClipWindingToWinding_full_inside) {
  // Clip a small quad completely inside a larger quad
  cm_winding_t *large = Cm_AllocWinding(4);
  large->num_points = 4;
  large->points[0] = Vec3(0, 0, 0);
  large->points[1] = Vec3(100, 0, 0);
  large->points[2] = Vec3(100, 0, 100);
  large->points[3] = Vec3(0, 0, 100);

  cm_winding_t *small = Cm_AllocWinding(4);
  small->num_points = 4;
  small->points[0] = Vec3(25, 0, 25);
  small->points[1] = Vec3(75, 0, 25);
  small->points[2] = Vec3(75, 0, 75);
  small->points[3] = Vec3(25, 0, 75);

  cm_winding_t *result = Cm_ClipWindingToWinding(small, large, Vec3(0, 1, 0), SIDE_EPSILON);

  ck_assert_ptr_nonnull(result);
  ck_assert_int_eq(4, result->num_points);
  
  // Should be unchanged since it's fully inside
  for (int32_t i = 0; i < 4; i++) {
    ck_assert(Vec3_EqualEpsilon(small->points[i], result->points[i], 0.01f));
  }

  Cm_FreeWinding(large);
  Cm_FreeWinding(small);
  Cm_FreeWinding(result);

} END_TEST

START_TEST(check_Cm_ClipWindingToWinding_full_outside) {
  // Clip a quad completely outside another quad
  cm_winding_t *clip = Cm_AllocWinding(4);
  clip->num_points = 4;
  clip->points[0] = Vec3(0, 0, 0);
  clip->points[1] = Vec3(50, 0, 0);
  clip->points[2] = Vec3(50, 0, 50);
  clip->points[3] = Vec3(0, 0, 50);

  cm_winding_t *outside = Cm_AllocWinding(4);
  outside->num_points = 4;
  outside->points[0] = Vec3(100, 0, 100);
  outside->points[1] = Vec3(150, 0, 100);
  outside->points[2] = Vec3(150, 0, 150);
  outside->points[3] = Vec3(100, 0, 150);

  cm_winding_t *result = Cm_ClipWindingToWinding(outside, clip, Vec3(0, 1, 0), SIDE_EPSILON);

  ck_assert_ptr_null(result);

  Cm_FreeWinding(clip);
  Cm_FreeWinding(outside);

} END_TEST

START_TEST(check_Cm_ClipWindingToWinding_partial_overlap) {
  // Clip a quad partially overlapping another
  cm_winding_t *clip = Cm_AllocWinding(4);
  clip->num_points = 4;
  clip->points[0] = Vec3(0, 0, 0);
  clip->points[1] = Vec3(50, 0, 0);
  clip->points[2] = Vec3(50, 0, 50);
  clip->points[3] = Vec3(0, 0, 50);

  cm_winding_t *overlap = Cm_AllocWinding(4);
  overlap->num_points = 4;
  overlap->points[0] = Vec3(25, 0, 25);
  overlap->points[1] = Vec3(75, 0, 25);
  overlap->points[2] = Vec3(75, 0, 75);
  overlap->points[3] = Vec3(25, 0, 75);

  cm_winding_t *result = Cm_ClipWindingToWinding(overlap, clip, Vec3(0, 1, 0), SIDE_EPSILON);

  ck_assert_ptr_nonnull(result);
  ck_assert_int_ge(result->num_points, 3); // At least a triangle
  
  // Verify all result points are within the clip bounds
  for (int32_t i = 0; i < result->num_points; i++) {
    ck_assert_float_ge(result->points[i].x, -SIDE_EPSILON);
    ck_assert_float_le(result->points[i].x, 50.f + SIDE_EPSILON);
    ck_assert_float_ge(result->points[i].z, -SIDE_EPSILON);
    ck_assert_float_le(result->points[i].z, 50.f + SIDE_EPSILON);
  }

  Cm_FreeWinding(clip);
  Cm_FreeWinding(overlap);
  Cm_FreeWinding(result);

} END_TEST

START_TEST(check_Cm_ClipWindingToWinding_triangle) {
  // Clip a quad to a triangular region
  cm_winding_t *triangle = Cm_AllocWinding(3);
  triangle->num_points = 3;
  triangle->points[0] = Vec3(0, 0, 0);
  triangle->points[1] = Vec3(100, 0, 0);
  triangle->points[2] = Vec3(50, 0, 100);

  cm_winding_t *quad = Cm_AllocWinding(4);
  quad->num_points = 4;
  quad->points[0] = Vec3(10, 0, 10);
  quad->points[1] = Vec3(90, 0, 10);
  quad->points[2] = Vec3(90, 0, 90);
  quad->points[3] = Vec3(10, 0, 90);

  cm_winding_t *result = Cm_ClipWindingToWinding(quad, triangle, Vec3(0, 1, 0), SIDE_EPSILON);

  ck_assert_ptr_nonnull(result);
  ck_assert_int_ge(result->num_points, 3); // At least a triangle

  Cm_FreeWinding(triangle);
  Cm_FreeWinding(quad);
  Cm_FreeWinding(result);

} END_TEST

START_TEST(check_Cm_ClipWindingToWinding_vertical_plane) {
  // Test clipping on a vertical plane (wall)
  cm_winding_t *clip = Cm_AllocWinding(4);
  clip->num_points = 4;
  clip->points[0] = Vec3(0, 0, 0);
  clip->points[1] = Vec3(0, 0, 100);
  clip->points[2] = Vec3(0, 100, 100);
  clip->points[3] = Vec3(0, 100, 0);
  
  cm_winding_t *in = Cm_AllocWinding(4);
  in->num_points = 4;
  in->points[0] = Vec3(0, 25, 25);
  in->points[1] = Vec3(0, 25, 75);
  in->points[2] = Vec3(0, 75, 75);
  in->points[3] = Vec3(0, 75, 25);
  
  cm_winding_t *result = Cm_ClipWindingToWinding(in, clip, Vec3(1, 0, 0), SIDE_EPSILON);
  
  ck_assert_ptr_nonnull(result);
  ck_assert_int_eq(4, result->num_points);
  
  // Result should be the same as input (fully inside)
  for (int32_t i = 0; i < 4; i++) {
    ck_assert(Vec3_EqualEpsilon(in->points[i], result->points[i], 0.01f));
  }
  
  Cm_FreeWinding(clip);
  Cm_FreeWinding(in);
  Cm_FreeWinding(result);

} END_TEST

START_TEST(check_Cm_ClipWindingToWinding_diagonal_plane) {
  // Test clipping on a diagonal plane
  const vec3_t normal = Vec3_Normalize(Vec3(1, 0, 1));
  
  // Create a square on the diagonal plane
  const vec3_t tangent = Vec3_Normalize(Vec3(-1, 0, 1));
  const vec3_t bitangent = Vec3(0, 1, 0);
  
  cm_winding_t *clip = Cm_AllocWinding(4);
  clip->num_points = 4;
  clip->points[0] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, -50), Vec3_Scale(bitangent, -50)), Vec3_Zero());
  clip->points[1] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, 50), Vec3_Scale(bitangent, -50)), Vec3_Zero());
  clip->points[2] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, 50), Vec3_Scale(bitangent, 50)), Vec3_Zero());
  clip->points[3] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, -50), Vec3_Scale(bitangent, 50)), Vec3_Zero());
  
  // Small square in the center
  cm_winding_t *in = Cm_AllocWinding(4);
  in->num_points = 4;
  in->points[0] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, -10), Vec3_Scale(bitangent, -10)), Vec3_Zero());
  in->points[1] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, 10), Vec3_Scale(bitangent, -10)), Vec3_Zero());
  in->points[2] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, 10), Vec3_Scale(bitangent, 10)), Vec3_Zero());
  in->points[3] = Vec3_Add(Vec3_Add(Vec3_Scale(tangent, -10), Vec3_Scale(bitangent, 10)), Vec3_Zero());
  
  cm_winding_t *result = Cm_ClipWindingToWinding(in, clip, normal, SIDE_EPSILON);
  
  ck_assert_ptr_nonnull(result);
  ck_assert_int_eq(4, result->num_points);
  
  Cm_FreeWinding(clip);
  Cm_FreeWinding(in);
  Cm_FreeWinding(result);

} END_TEST

START_TEST(check_Cm_ClipWindingToWinding_offset_planes) {
  // Test clipping windings that are offset from each other
  cm_winding_t *clip = Cm_AllocWinding(4);
  clip->num_points = 4;
  clip->points[0] = Vec3(0, 1, 0);
  clip->points[1] = Vec3(100, 1, 0);
  clip->points[2] = Vec3(100, 1, 100);
  clip->points[3] = Vec3(0, 1, 100);
  
  // Decal winding offset by 1 unit in normal direction
  cm_winding_t *in = Cm_AllocWinding(4);
  in->num_points = 4;
  in->points[0] = Vec3(25, 2, 25);
  in->points[1] = Vec3(75, 2, 25);
  in->points[2] = Vec3(75, 2, 75);
  in->points[3] = Vec3(25, 2, 75);
  
  // This should still work if we use a large enough epsilon
  cm_winding_t *result = Cm_ClipWindingToWinding(in, clip, Vec3(0, 1, 0), 2.0);
  
  ck_assert_ptr_nonnull(result);
  ck_assert_int_eq(4, result->num_points);
  
  Cm_FreeWinding(clip);
  Cm_FreeWinding(in);
  Cm_FreeWinding(result);

} END_TEST

START_TEST(check_Cm_ClipWindingToWinding_decal_scenario) {
  // Simulate actual decal clipping scenario
  // Face on a floor
  cm_winding_t *face = Cm_AllocWinding(4);
  face->num_points = 4;
  face->points[0] = Vec3(-64, 0, -64);
  face->points[1] = Vec3(64, 0, -64);
  face->points[2] = Vec3(64, 0, 64);
  face->points[3] = Vec3(-64, 0, 64);
  
  // Decal quad in center of face
  cm_winding_t *decal = Cm_AllocWinding(4);
  decal->num_points = 4;
  decal->points[0] = Vec3(-16, 0, -16);
  decal->points[1] = Vec3(16, 0, -16);
  decal->points[2] = Vec3(16, 0, 16);
  decal->points[3] = Vec3(-16, 0, 16);
  
  cm_winding_t *result = Cm_ClipWindingToWinding(decal, face, Vec3(0, 1, 0), SIDE_EPSILON);
  
  ck_assert_ptr_nonnull(result);
  ck_assert_int_eq(4, result->num_points);
  
  // Should be unchanged since decal is fully inside face
  for (int32_t i = 0; i < 4; i++) {
    ck_assert(Vec3_EqualEpsilon(decal->points[i], result->points[i], 0.01f));
  }
  
  Cm_FreeWinding(face);
  Cm_FreeWinding(decal);
  Cm_FreeWinding(result);

} END_TEST

START_TEST(check_Cm_ClipWindingToWinding_edge_aligned) {
  // Test when decal edge aligns with face edge
  cm_winding_t *face = Cm_AllocWinding(4);
  face->num_points = 4;
  face->points[0] = Vec3(0, 0, 0);
  face->points[1] = Vec3(64, 0, 0);
  face->points[2] = Vec3(64, 0, 64);
  face->points[3] = Vec3(0, 0, 64);
  
  // Decal that extends to face edge
  cm_winding_t *decal = Cm_AllocWinding(4);
  decal->num_points = 4;
  decal->points[0] = Vec3(0, 0, 16);
  decal->points[1] = Vec3(32, 0, 16);
  decal->points[2] = Vec3(32, 0, 48);
  decal->points[3] = Vec3(0, 0, 48);
  
  cm_winding_t *result = Cm_ClipWindingToWinding(decal, face, Vec3(0, 1, 0), SIDE_EPSILON);
  
  ck_assert_ptr_nonnull(result);
  ck_assert_int_eq(4, result->num_points);
  
  Cm_FreeWinding(face);
  Cm_FreeWinding(decal);
  Cm_FreeWinding(result);

} END_TEST

START_TEST(check_Cm_DistanceToWinding) {

  cm_winding_t *w = Cm_AllocWinding(3);
  w->num_points = 3;

  w->points[0] = Vec3(0.f, 0.f, 0.f);
  w->points[1] = Vec3(0.f, 1.f, 0.f);
  w->points[2] = Vec3(1.f, 0.f, 0.f);

  vec3_t dir;
  ck_assert_float_eq(0.f, Cm_DistanceToWinding(w, w->points[0], &dir));
  ck_assert_float_eq(0.f, Cm_DistanceToWinding(w, w->points[1], &dir));
  ck_assert_float_eq(0.f, Cm_DistanceToWinding(w, w->points[2], &dir));

  ck_assert_float_eq(0.f, Cm_DistanceToWinding(w, Vec3(.5f, .5f, 0.f), &dir));
  ck_assert_float_eq(1.f, Cm_DistanceToWinding(w, Vec3(0.f, 2.f, 0.f), &dir));

  Cm_FreeWinding(w);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  Suite *suite = suite_create("check_cm_polylib");

  {
    TCase *tcase = tcase_create("Cm_ClipWinding");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Cm_ClipWinding_front);
    tcase_add_test(tcase, check_Cm_ClipWinding_back);
    tcase_add_test(tcase, check_Cm_ClipWinding_both);
    tcase_add_test(tcase, check_Cm_ClipWinding_on);
    suite_add_tcase(suite, tcase);
  }

  {
    TCase *tcase = tcase_create("Cm_ElementsForWinding");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Cm_ElementsForWinding_triangle);
    tcase_add_test(tcase, check_Cm_ElementsForWinding_quad);
    tcase_add_test(tcase, check_Cm_ElementsForWinding_skinnyQuad);
    tcase_add_test(tcase, check_Cm_ElementsForWinding_colinearQuad);
    tcase_add_test(tcase, check_Cm_ElementsForWinding_cornerCase);
    tcase_add_test(tcase, check_Cm_ElementsForWinding_invalid);
    suite_add_tcase(suite, tcase);
  }

  {
    TCase *tcase = tcase_create("Cm_TriangleArea");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Cm_TriangleArea);
    suite_add_tcase(suite, tcase);
  }

  {
    TCase *tcase = tcase_create("Cm_Barycentric");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Cm_Barycentric);
    suite_add_tcase(suite, tcase);
  }

  {
    TCase *tcase = tcase_create("Cm_ClipWindingToWinding");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Cm_ClipWindingToWinding_full_inside);
    tcase_add_test(tcase, check_Cm_ClipWindingToWinding_full_outside);
    tcase_add_test(tcase, check_Cm_ClipWindingToWinding_partial_overlap);
    tcase_add_test(tcase, check_Cm_ClipWindingToWinding_triangle);
    tcase_add_test(tcase, check_Cm_ClipWindingToWinding_vertical_plane);
    tcase_add_test(tcase, check_Cm_ClipWindingToWinding_diagonal_plane);
    tcase_add_test(tcase, check_Cm_ClipWindingToWinding_offset_planes);
    tcase_add_test(tcase, check_Cm_ClipWindingToWinding_decal_scenario);
    tcase_add_test(tcase, check_Cm_ClipWindingToWinding_edge_aligned);
    suite_add_tcase(suite, tcase);
  }

  {
    TCase *tcase = tcase_create("Cm_DistanceToWinding");
    tcase_add_checked_fixture(tcase, setup, teardown);
    tcase_add_test(tcase, check_Cm_DistanceToWinding);
    suite_add_tcase(suite, tcase);
  }

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
