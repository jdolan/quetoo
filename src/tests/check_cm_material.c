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
#include "collision/cm_material.h"

Quetoo quetoo;

/**
 * @brief Setup fixture.
 */
void setup(void) {
	Mem_Init();
	Fs_Init(FS_NONE);
	ck_assert(Fs_SetGame(TEST_GAME, NULL));
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {
	Fs_Shutdown();
	Mem_Shutdown();
}

/**
 * @brief Helper to write raw text to a file.
 */
static void write_file(const char *path, const char *content) {
	File *file = Fs_OpenWrite(path);
	ck_assert_msg(file != NULL, "Failed to open %s for writing", path);
	Fs_Print(file, "%s", content);
	Fs_Close(file);
}

START_TEST(check_Cm_LoadMaterial_light) {

	write_file("check_light.mat",
		"{\n"
		"	{\n"
		"		texture check_light\n"
		"		pulse 1.5\n"
		"		light.radius 200\n"
		"		light.color 1 0 1\n"
		"		light.intensity 1.5\n"
		"	}\n"
		"	{\n"
		"		texture check_light\n"
		"		light.intensity 2\n"
		"	}\n"
		"}\n"
	);

	CmMaterial *m = Cm_LoadMaterial("check_light", ASSET_CONTEXT_NONE);
	ck_assert_ptr_nonnull(m);
	ck_assert(m->stageFlags & STAGE_LIGHT);

	const CmStage *s = m->stages;
	ck_assert_ptr_nonnull(s);
	ck_assert(s->flags & STAGE_LIGHT);
	ck_assert(s->flags & STAGE_PULSE);
	ck_assert_float_eq(s->pulse.hz, 1.5f);
	ck_assert_float_eq(s->light.radius, 200.f);
	ck_assert(Vec3_Equal(s->light.color, MakeVec3(1.f, 0.f, 1.f)));
	ck_assert_float_eq(s->light.intensity, 1.5f);

	s = s->next;
	ck_assert_ptr_nonnull(s);
	ck_assert(s->flags & STAGE_LIGHT);
	ck_assert_float_eq(s->light.radius, STAGE_LIGHT_RADIUS);
	ck_assert(Vec3_Equal(s->light.color, Vec3_Zero()));
	ck_assert_float_eq(s->light.intensity, 2.f);

	Cm_FreeMaterial(m);

} END_TEST

START_TEST(check_Cm_SaveMaterial_light) {

	write_file("check_save.mat",
		"{\n"
		"	{\n"
		"		texture check_save\n"
		"		light.radius 150\n"
		"		light.color 0.5 0.25 1\n"
		"		light.intensity 0.75\n"
		"	}\n"
		"}\n"
	);

	CmMaterial *m = Cm_LoadMaterial("check_save", ASSET_CONTEXT_NONE);
	ck_assert_ptr_nonnull(m);
	ck_assert(Cm_SaveMaterial(m));
	Cm_FreeMaterial(m);

	m = Cm_LoadMaterial("check_save", ASSET_CONTEXT_NONE);
	ck_assert_ptr_nonnull(m);

	const CmStage *s = m->stages;
	ck_assert_ptr_nonnull(s);
	ck_assert(s->flags & STAGE_LIGHT);
	ck_assert_float_eq(s->light.radius, 150.f);
	ck_assert(Vec3_Equal(s->light.color, MakeVec3(.5f, .25f, 1.f)));
	ck_assert_float_eq(s->light.intensity, .75f);

	Cm_FreeMaterial(m);

} END_TEST

START_TEST(check_Cm_LoadMaterial_pulse_drift_ignored) {

	write_file("check_drift.mat",
		"{\n"
		"	{\n"
		"		texture check_drift\n"
		"		pulse 2.00 1.000\n"
		"		emissive 0.5\n"
		"	}\n"
		"}\n"
	);

	CmMaterial *m = Cm_LoadMaterial("check_drift", ASSET_CONTEXT_NONE);
	ck_assert_ptr_nonnull(m);

	const CmStage *s = m->stages;
	ck_assert_ptr_nonnull(s);
	ck_assert(s->flags & STAGE_PULSE);
	ck_assert_float_eq(s->pulse.hz, 2.f);
	ck_assert(s->flags & STAGE_EMISSIVE);
	ck_assert_float_eq(s->emissive, .5f);

	Cm_FreeMaterial(m);

} END_TEST

START_TEST(check_Cm_ResolveStageFlags) {

	write_file("check_flags.mat",
		"{\n"
		"	{\n"
		"		texture check_flags\n"
		"		light.intensity 1\n"
		"	}\n"
		"}\n"
	);

	CmMaterial *m = Cm_LoadMaterial("check_flags", ASSET_CONTEXT_NONE);
	ck_assert_ptr_nonnull(m);
	ck_assert(m->stageFlags & STAGE_LIGHT);

	m->stages->flags &= ~STAGE_LIGHT;
	Cm_ResolveStageFlags(m);
	ck_assert(!(m->stageFlags & STAGE_LIGHT));

	Cm_FreeMaterial(m);

} END_TEST

START_TEST(check_Cm_LoadMaterial_light_only_stage) {

	write_file("check_only.mat",
		"{\n"
		"	{\n"
		"		light.radius 200\n"
		"		light.intensity 1.5\n"
		"	}\n"
		"	{\n"
		"		texture check_only\n"
		"		pulse 1\n"
		"	}\n"
		"}\n"
	);

	CmMaterial *m = Cm_LoadMaterial("check_only", ASSET_CONTEXT_NONE);
	ck_assert_ptr_nonnull(m);

	const CmStage *s = Cm_MaterialLightStage(m);
	ck_assert_ptr_eq(s, m->stages);
	ck_assert(!(s->flags & STAGE_DRAW));
	ck_assert_float_eq(s->light.radius, 200.f);

	ck_assert_ptr_nonnull(m->stages->next);
	ck_assert(m->stages->next->flags & STAGE_PULSE);

	Cm_FreeMaterial(m);

} END_TEST

START_TEST(check_Cm_AddStage_RemoveStage) {

	write_file("check_edit.mat",
		"{\n"
		"	diffusemap check_edit\n"
		"	{\n"
		"		texture check_edit\n"
		"		pulse 1\n"
		"	}\n"
		"}\n"
	);

	CmMaterial *m = Cm_LoadMaterial("check_edit", ASSET_CONTEXT_NONE);
	ck_assert_ptr_nonnull(m);
	ck_assert(!(m->stageFlags & STAGE_LIGHT));

	CmStage *s = Cm_AddStage(m);
	ck_assert_ptr_nonnull(s);
	ck_assert_ptr_eq(m->stages->next, s);
	ck_assert(s->flags & STAGE_TEXTURE);
	ck_assert_str_eq(s->asset.name, "check_edit");
	ck_assert(m->dirty);

	s->flags |= STAGE_LIGHT;
	s->light.intensity = 3.f;
	Cm_ResolveStage(m, s);
	ck_assert(m->stageFlags & STAGE_LIGHT);
	ck_assert_float_eq(s->light.radius, STAGE_LIGHT_RADIUS);
	ck_assert_ptr_eq(Cm_MaterialLightStage(m), s);

	Cm_RemoveStage(m, s);
	ck_assert_ptr_null(m->stages->next);
	ck_assert(!(m->stageFlags & STAGE_LIGHT));
	ck_assert_ptr_null(Cm_MaterialLightStage(m));

	Cm_FreeMaterial(m);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	Test_Init(argc, argv);

	TCase *tcase = tcase_create("check_cm_material");
	tcase_add_checked_fixture(tcase, setup, teardown);

	tcase_add_test(tcase, check_Cm_LoadMaterial_light);
	tcase_add_test(tcase, check_Cm_SaveMaterial_light);
	tcase_add_test(tcase, check_Cm_LoadMaterial_pulse_drift_ignored);
	tcase_add_test(tcase, check_Cm_ResolveStageFlags);
	tcase_add_test(tcase, check_Cm_LoadMaterial_light_only_stage);
	tcase_add_test(tcase, check_Cm_AddStage_RemoveStage);

	Suite *suite = suite_create("check_cm_material");
	suite_add_tcase(suite, tcase);

	int32_t failed = Test_Run(suite);

	Test_Shutdown();
	return failed;
}
