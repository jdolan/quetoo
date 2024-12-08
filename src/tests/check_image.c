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

#include <check.h>
#include <unistd.h>

#include "tests.h"

quetoo_t quetoo;

/**
 * @brief Setup fixture.
 */
static void setup(void) {

	Mem_Init();

	Fs_Init(FS_AUTO_LOAD_ARCHIVES);
}

/**
 * @brief Teardown fixture.
 */
static void teardown(void) {

	Fs_Shutdown();

	Mem_Shutdown();
}

START_TEST(check_Img_CreateHeightmap) {

	SDL_Surface *diffusemap = Img_LoadSurface("textures/pits/lead1_1");
	ck_assert_ptr_nonnull(diffusemap);

	SDL_Surface *normalmap = Img_LoadSurface("textures/pits/lead1_1");
	ck_assert_ptr_nonnull(normalmap);

	SDL_Surface *heightmap = SDL_CreateRGBSurfaceWithFormat(0, normalmap->w, normalmap->h, 32, SDL_PIXELFORMAT_RGBA32);
	ck_assert_ptr_nonnull(heightmap);

	Img_CreateHeightmap(diffusemap, normalmap, heightmap);

	IMG_SavePNG(normalmap, "/tmp/normalmap.png");
	IMG_SavePNG(heightmap, "/tmp/heightmap.png");

	SDL_FreeSurface(normalmap);
	SDL_FreeSurface(heightmap);
} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	TCase *tcase = tcase_create("check_Img_CreateHeightmap");
	tcase_add_checked_fixture(tcase, setup, teardown);

	tcase_add_test(tcase, check_Img_CreateHeightmap);

	Suite *suite = suite_create("check_image");
	suite_add_tcase(suite, tcase);

	SRunner *runner = srunner_create(suite);

	srunner_run_all(runner, CK_VERBOSE);
	int32_t failed = srunner_ntests_failed(runner);

	srunner_free(runner);
	return failed;
}
