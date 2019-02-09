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

#include "atlas.h"

static SDL_Surface *CreateSurface(int32_t w, int32_t h, int32_t color) {

	SDL_Surface *surface = SDL_CreateRGBSurface(0, w, h, 24, 0, 0, 0, 0);

	SDL_FillRect(surface, &(SDL_Rect) {
		0, 0, surface->w, surface->h
	}, color);

	return surface;
}

START_TEST(check_atlas) {

	SDL_Surface *red = CreateSurface(128, 128, 0xff0000);
	SDL_Surface *green = CreateSurface(256, 256, 0x00ff00);
	SDL_Surface *blue = CreateSurface(512, 512, 0x0000ff);
	SDL_Surface *purple = CreateSurface(512, 512, 0xff00ff);

	atlas_t *atlas = Atlas_Create(1);

	atlas_node_t *a = Atlas_Insert(atlas, red);
	ck_assert_ptr_ne(NULL, a);

	atlas_node_t *b = Atlas_Insert(atlas, green);
	ck_assert_ptr_ne(NULL, b);

	atlas_node_t *c = Atlas_Insert(atlas, blue);
	ck_assert_ptr_ne(NULL, c);

	SDL_Surface *surface = CreateSurface(1024, 1024, 0x000000);

	int32_t res = Atlas_Compile(atlas, 0, surface);
	ck_assert_int_eq(0, res);

	ck_assert_int_eq(0, c->x);
	ck_assert_int_eq(0, c->y);

	ck_assert_int_eq(512, b->x);
	ck_assert_int_eq(0, b->y);

	ck_assert_int_eq(768, a->x);
	ck_assert_int_eq(0, a->y);

	atlas_node_t *d = Atlas_Insert(atlas, purple);
	ck_assert_ptr_ne(NULL, d);

	res = Atlas_Compile(atlas, 0, surface);
	ck_assert_int_eq(0, res);

	ck_assert_int_eq(0, c->x);
	ck_assert_int_eq(0, c->y);

	ck_assert_int_eq(512, d->x);
	ck_assert_int_eq(0, d->y);

	ck_assert_int_eq(0, b->x);
	ck_assert_int_eq(512, b->y);

	ck_assert_int_eq(256, a->x);
	ck_assert_int_eq(512, a->y);

	Atlas_Destroy(atlas);

	SDL_FreeSurface(red);
	SDL_FreeSurface(green);
	SDL_FreeSurface(blue);
	SDL_FreeSurface(purple);

	IMG_SavePNG(surface, "/tmp/check_atlas.png");

	SDL_FreeSurface(surface);

} END_TEST

START_TEST(check_atlas_random) {

	srand(getpid());

	atlas_t *atlas = Atlas_Create(1);

	SDL_Surface *surfaces[100];

	for (size_t i = 0; i < 100; i++) {

		const int32_t w = rand() % 96 + 1;
		const int32_t h = rand() % 96 + 1;
		const int32_t color = rand() % 255 << 16 | rand() % 255 << 8 | rand() % 255;
		
		surfaces[i] = CreateSurface(w, h, color);

		Atlas_Insert(atlas, surfaces[i]);
	}

	SDL_Surface *surface = CreateSurface(1024, 1024, 0);

	const int32_t res = Atlas_Compile(atlas, 0, surface);
	ck_assert_int_eq(0, res);

	Atlas_Destroy(atlas);

	IMG_SavePNG(surface, "/tmp/check_atlas_random.png");

	for (size_t i = 0; i < 100; i++) {
		SDL_FreeSurface(surfaces[i]);
	}

	SDL_FreeSurface(surface);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	TCase *tcase = tcase_create("check_atlas");

	tcase_add_test(tcase, check_atlas);
	tcase_add_test(tcase, check_atlas_random);

	Suite *suite = suite_create("check_atlas");
	suite_add_tcase(suite, tcase);

	SRunner *runner = srunner_create(suite);

	srunner_run_all(runner, CK_NORMAL);
	int32_t failed = srunner_ntests_failed(runner);

	srunner_free(runner);
	return failed;
}
