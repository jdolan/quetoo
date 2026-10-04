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
#include "r_local.h"

Quetoo quetoo;

Cvar *developer;
Cvar *editor;

static bool Test_WaitForIdle(const RenderDevice *self) {
  return true;
}

/**
 * @brief Setup fixture.
 */
void setup(void) {
  static Cvar nullCvar;

  // Objectively dispatches through the instance's Class, so a stub device needs one
  static RenderDeviceInterface interface = { .waitForIdle = Test_WaitForIdle };
  static Class clazz = { .interface = &interface };
  static RenderDevice device = { .object = { .clazz = &clazz } };

  developer = &nullCvar;
  editor = &nullCvar;

  Mem_Init();

  renderContext.device = &device;
  renderOcclusion.boxes = $(alloc(Vector), initWithSize, sizeof(Box3));

  R_InitMedia();
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {

  R_ShutdownMedia();

  renderOcclusion.boxes = release(renderOcclusion.boxes);
  renderContext.device = NULL;

  Mem_Shutdown();
}

START_TEST(check_R_RegisterMedia) {
  R_BeginLoading();

  RenderMedia *parent1 = R_AllocMedia("parent1", sizeof(RenderMedia), R_MEDIA_GENERIC);
  R_RegisterMedia(parent1);

  ck_assert_msg(R_FindMedia("parent1", R_MEDIA_GENERIC) == parent1, "Failed to find parent1");

  RenderMedia *child1 = R_AllocMedia("child1", sizeof(RenderMedia), R_MEDIA_GENERIC);
  R_RegisterDependency(parent1, child1);

  ck_assert_msg(R_FindMedia("child1", R_MEDIA_GENERIC) == child1, "Failed to find child1");

  RenderMedia *grandchild1 = R_AllocMedia("grandchild1", sizeof(RenderMedia), R_MEDIA_GENERIC);
  R_RegisterDependency(child1, grandchild1);

  R_EndLoading();

  ck_assert_msg(R_FindMedia("parent1", R_MEDIA_GENERIC) == parent1, "Erroneously freed parent1");
  ck_assert_msg(R_FindMedia("child1", R_MEDIA_GENERIC) == child1, "Erroneously freed child1");
  ck_assert_msg(R_FindMedia("grandchild1", R_MEDIA_GENERIC) == grandchild1, "Erroneously freed grandchild1");

  int32_t oldSeed = parent1->seed;

  R_BeginLoading();

  ck_assert(R_FindMedia("parent1", R_MEDIA_GENERIC) == parent1);
  ck_assert_msg(oldSeed != parent1->seed, "Seed for parent1 has not changed");
  ck_assert_msg(child1->seed == parent1->seed, "Dependency child1 not retained");
  ck_assert_msg(grandchild1->seed == parent1->seed, "Dependency grandchild1 not retained");

  R_BeginLoading();
  R_EndLoading();

  ck_assert_msg(Mem_Size() == 0, "Not all memory freed: %u", (uint32_t) Mem_Size());

  R_BeginLoading();

  RenderMedia *copy0 = R_AllocMedia("copy", sizeof(RenderMedia), R_MEDIA_GENERIC);
  R_RegisterMedia(copy0);

  ck_assert_msg(R_FindMedia("copy", R_MEDIA_GENERIC) == copy0, "Failed to find copy0");

  RenderMedia *copy1 = R_AllocMedia("copy", sizeof(RenderMedia), R_MEDIA_GENERIC);
  R_RegisterMedia(copy1);

  ck_assert_msg(R_FindMedia("copy", R_MEDIA_GENERIC) == copy1, "Failed to replace copy0 with copy1");

  R_BeginLoading();
  R_EndLoading();

  ck_assert_msg(Mem_Size() == 0, "Not all memory freed: %u", (uint32_t) Mem_Size());

} END_TEST

START_TEST(check_R_FreeMedia) {
  R_BeginLoading();

  RenderMedia *media = R_AllocMedia("free_me", sizeof(RenderMedia), R_MEDIA_GENERIC);
  R_RegisterMedia(media);

  ck_assert_msg(R_FindMedia("free_me", R_MEDIA_GENERIC) == media, "Failed to find free_me before free");

  R_FreeMedia(media);

  ck_assert_msg(R_FindMedia("free_me", R_MEDIA_GENERIC) == NULL, "free_me should not be findable after R_FreeMedia");

  R_BeginLoading();
  R_EndLoading();

  ck_assert_msg(Mem_Size() == 0, "Not all memory freed: %u", (uint32_t) Mem_Size());

} END_TEST

START_TEST(check_R_QualityBudgets) {
  const int32_t levels[] = { -1, 0, 1, 2, 3, 4 };
  const float scales[] = { .25f, .25f, .25f, .5f, 1.f, 1.f };

  for (size_t i = 0; i < lengthof(levels); i++) {
    const Cvar quality = { .integer = levels[i] };
    ck_assert_float_eq(scales[i], R_QualityScale(&quality));
    ck_assert_int_eq(clamp(levels[i], 1, 3), R_QualityLevel(&quality));
    ck_assert_int_eq((int32_t) (scales[i] * 8), 2 << (R_QualityLevel(&quality) - 1));
    ck_assert_int_eq((int32_t) (scales[i] * 64), 16 << (R_QualityLevel(&quality) - 1));
    ck_assert_int_eq((int32_t) (scales[i] * 16), 4 << (R_QualityLevel(&quality) - 1));
  }

  ck_assert_int_eq(416, sizeof(renderUniforms.block));
  ck_assert_int_eq(400, offsetof(struct RenderUniformBlock, shadowSamples));
  ck_assert_int_eq(404, offsetof(struct RenderUniformBlock, parallaxSamples));
  ck_assert_int_eq(408, offsetof(struct RenderUniformBlock, parallaxShadowSamples));
  ck_assert_int_eq(412, offsetof(struct RenderUniformBlock, parallaxRefineSteps));
} END_TEST

START_TEST(check_R_QualityUniforms) {
  Cvar **variables[] = {
    &r_ambient, &r_modulate, &r_saturation, &r_caustics, &r_ambientOcclusion,
    &r_lightingDistance, &r_drawWireframe, &r_parallaxShadow, &r_shadowQuality,
    &r_parallaxQuality, &r_parallaxShadowQuality, &r_parallax, &r_alphaTest,
    &r_roughness, &r_hardness, &r_specularity,
  };
  Cvar *previous[lengthof(variables)];
  Cvar values[lengthof(variables)];
  for (size_t i = 0; i < lengthof(variables); i++) {
    previous[i] = *variables[i];
    values[i] = (Cvar) { .integer = 1, .value = 1.f };
    *variables[i] = &values[i];
  }

  static RenderView view;
  view.type = VIEW_PLAYER_MODEL;
  view.viewport = (Vec4i) { .z = 1920, .w = 1080 };
  view.fov = MakeVec2(45.f, 35.f);
  view.forward = MakeVec3(1.f, 0.f, 0.f);
  view.up = MakeVec3(0.f, 0.f, 1.f);

  Material def = { .parallax = 8.f };
  const RenderMaterial material = { .def = &def };
  RenderMaterialUniforms uniforms;

  for (int32_t level = 1; level <= 3; level++) {
    r_shadowQuality->integer = level;
    r_parallaxQuality->integer = level;
    r_parallaxShadowQuality->integer = level;
    R_UpdateUniforms(&view);
    ck_assert_int_eq(2 << (level - 1), renderUniforms.block.shadowSamples);
    ck_assert_float_eq(.25f * (1 << (level - 1)), renderUniforms.block.parallaxSamples);
    ck_assert_float_eq(.25f * (1 << (level - 1)), renderUniforms.block.parallaxShadowSamples);
    ck_assert_int_eq(1 << (level - 1), renderUniforms.block.parallaxRefineSteps);

    r_parallax->value = .6f;
    R_MaterialUniforms(&material, 0, &uniforms);
    ck_assert_float_eq_tol(.25f * (level + 1),
      uniforms.parallax * uniforms.parallax / (8.f * 8.f * .6f * .6f), .00001f);
    r_parallax->value = 0.f;
    R_MaterialUniforms(&material, 0, &uniforms);
    ck_assert_float_eq(0.f, uniforms.parallax);
  }

  for (size_t i = 0; i < lengthof(variables); i++) {
    *variables[i] = previous[i];
  }
} END_TEST

START_TEST(check_R_SubviewResolutions) {
  SDL_DisplayMode display = { .pixel_density = 1.f };
  Cvar scale = { .value = 1.f };
  const SDL_DisplayMode *previousDisplay = renderContext.displayMode;
  Cvar *previousScale = r_framebufferScale;
  renderContext.displayMode = &display;
  r_framebufferScale = &scale;

  const SDL_Size window = MakeSize(1919, 1079);
  const SDL_Size expected[] = { MakeSize(479, 269), MakeSize(639, 359), MakeSize(959, 539) };

  for (int32_t i = 0; i < 3; i++) {
    Cvar quality = { .integer = i + 1 };
    SDL_Size size = R_SubviewSize(window, &quality);
    ck_assert_int_eq(expected[i].w, size.w);
    ck_assert_int_eq(expected[i].h, size.h);

    display.pixel_density = 2.f;
    scale.value = .75f;
    size = R_SubviewSize(window, &quality);
    ck_assert_int_eq((int32_t) (expected[i].w * 1.5f), size.w);
    ck_assert_int_eq((int32_t) (expected[i].h * 1.5f), size.h);
    display.pixel_density = scale.value = 1.f;

    size = R_SubviewSize(MakeSize(1, 1), &quality);
    ck_assert_int_eq(1, size.w);
    ck_assert_int_eq(1, size.h);
  }

  Cvar quality = { .integer = 100 };
  SDL_Size size = R_SubviewSize(window, &quality);
  ck_assert_int_eq(959, size.w);
  quality.integer = -100;
  size = R_SubviewSize(window, &quality);
  ck_assert_int_eq(479, size.w);

  renderContext.displayMode = previousDisplay;
  r_framebufferScale = previousScale;
} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  TCase *tcase = tcase_create("check_r_media");
  tcase_add_checked_fixture(tcase, setup, teardown);

  tcase_add_test(tcase, check_R_RegisterMedia);
  tcase_add_test(tcase, check_R_FreeMedia);
  tcase_add_test(tcase, check_R_QualityBudgets);
  tcase_add_test(tcase, check_R_QualityUniforms);
  tcase_add_test(tcase, check_R_SubviewResolutions);

  Suite *suite = suite_create("check_r_media");
  suite_add_tcase(suite, tcase);

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
