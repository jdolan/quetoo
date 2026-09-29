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
#include "common/manifest.h"

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

START_TEST(check_Manifest_Read) {

	write_file("test.mf",
		"d41d8cd98f00b204e9800998ecf8427e 1234 textures/edge/floor01_d.tga\n"
		"a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6 5678 sounds/weapons/rg_fire.ogg\n"
	);

	HashTable *manifest = Manifest_Read("test.mf");
	ck_assert_msg(manifest != NULL, "Manifest_Read returned NULL");
	ck_assert_int_eq((manifest)->count, 2);

	const ManifestEntry *e0 = $(manifest, get, (void *) "textures/edge/floor01_d.tga");
	ck_assert_msg(e0 != NULL, "Missing textures/edge/floor01_d.tga");
	ck_assert_str_eq(e0->hash, "d41d8cd98f00b204e9800998ecf8427e");
	ck_assert_int_eq(e0->size, 1234);
	ck_assert_str_eq(e0->path, "textures/edge/floor01_d.tga");

	const ManifestEntry *e1 = $(manifest, get, (void *) "sounds/weapons/rg_fire.ogg");
	ck_assert_msg(e1 != NULL, "Missing sounds/weapons/rg_fire.ogg");
	ck_assert_str_eq(e1->hash, "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6");
	ck_assert_int_eq(e1->size, 5678);
	ck_assert_str_eq(e1->path, "sounds/weapons/rg_fire.ogg");

	Manifest_Free(manifest);

} END_TEST

START_TEST(check_Manifest_Read_empty_lines) {

	write_file("test_empty.mf",
		"\n"
		"d41d8cd98f00b204e9800998ecf8427e 100 textures/foo.tga\n"
		"\n"
		"\n"
		"a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6 200 sounds/bar.ogg\n"
		"\n"
	);

	HashTable *manifest = Manifest_Read("test_empty.mf");
	ck_assert_msg(manifest != NULL, "Manifest_Read returned NULL");
	ck_assert_int_eq((manifest)->count, 2);

	Manifest_Free(manifest);

} END_TEST

START_TEST(check_Manifest_Read_missing_file) {

	HashTable *manifest = Manifest_Read("nonexistent.mf");
	ck_assert_msg(manifest == NULL, "Expected NULL for missing manifest");

} END_TEST

START_TEST(check_Manifest_Parse_respects_len) {

	// Simulates a buffer from Net_HttpGet, which is not null-terminated: the
	// valid manifest is followed by stale bytes that look like another (bogus)
	// entry. Manifest_Parse must honor `len` and never read past it.
	static const char valid[] =
		"d41d8cd98f00b204e9800998ecf8427e 1234 textures/edge/floor01_d.tga\n";
	static const char garbage[] =
		"deadbeefdeadbeefdeadbeefdeadbeef 9999 textures/overrun.tga\n";

	const size_t len = sizeof(valid) - 1; // valid content only, no null terminator

	char *buffer = Mem_Malloc(len + sizeof(garbage));
	memcpy(buffer, valid, len);
	memcpy(buffer + len, garbage, sizeof(garbage)); // trailing bytes past `len`

	HashTable *manifest = Manifest_Parse(buffer, len);
	ck_assert_msg(manifest != NULL, "Manifest_Parse returned NULL");
	ck_assert_int_eq((manifest)->count, 1);
	ck_assert_msg($(manifest, get, (void *) "textures/edge/floor01_d.tga") != NULL,
		"Missing valid entry");
	ck_assert_msg($(manifest, get, (void *) "textures/overrun.tga") == NULL,
		"Parsed an entry from beyond `len` (buffer over-read)");

	Manifest_Free(manifest);
	Mem_Free(buffer);

} END_TEST

START_TEST(check_Manifest_Write) {

	const char *content1 = "hello";
	const char *content2 = "world";

	HashTable *manifest = Manifest_Alloc();
	Manifest_AddEntry(manifest, "textures/edge/floor01_d.tga", content1, strlen(content1));
	Manifest_AddEntry(manifest, "sounds/weapons/rg_fire.ogg", content2, strlen(content2));

	const int32_t count = Manifest_Write("test_write.mf", manifest);
	ck_assert_int_eq(count, 2);

	Manifest_Free(manifest);

	// verify the file was written correctly (MD5 of "hello" and "world")
	void *data = NULL;
	const int64_t len = Fs_Load("test_write.mf", &data);
	ck_assert_msg(len > 0, "Failed to load written manifest");
	ck_assert_msg(strstr((const char *) data, "5d41402abc4b2a76b9719d911017c592 5 textures/edge/floor01_d.tga") != NULL,
		"Missing first entry in written manifest");
	ck_assert_msg(strstr((const char *) data, "7d793037a0760186574b0282f2f435e7 5 sounds/weapons/rg_fire.ogg") != NULL,
		"Missing second entry in written manifest");
	Fs_Free(data);

} END_TEST

START_TEST(check_Manifest_roundtrip) {

	const char *content1 = "file1data";
	const char *content2 = "file2data";
	const char *content3 = "file3data";

	HashTable *manifest = Manifest_Alloc();
	Manifest_AddEntry(manifest, "textures/edge/floor01_d.tga", content1, strlen(content1));
	Manifest_AddEntry(manifest, "sounds/weapons/rg_fire.ogg", content2, strlen(content2));
	Manifest_AddEntry(manifest, "maps/edge.nav", content3, strlen(content3));

	// save original entries for comparison
	const ManifestEntry origTex  = *((ManifestEntry *) $(manifest, get, (void *) "textures/edge/floor01_d.tga"));
	const ManifestEntry origSnd  = *((ManifestEntry *) $(manifest, get, (void *) "sounds/weapons/rg_fire.ogg"));
	const ManifestEntry origNav  = *((ManifestEntry *) $(manifest, get, (void *) "maps/edge.nav"));

	Manifest_Write("test_roundtrip.mf", manifest);
	Manifest_Free(manifest);

	HashTable *loaded = Manifest_Read("test_roundtrip.mf");
	ck_assert_msg(loaded != NULL, "Manifest_Read returned NULL after write");
	ck_assert_int_eq((loaded)->count, 3);

	const ManifestEntry *e;

	e = $(loaded, get, (void *) "textures/edge/floor01_d.tga");
	ck_assert_msg(e != NULL, "Missing textures/edge/floor01_d.tga after roundtrip");
	ck_assert_str_eq(e->hash, origTex.hash);
	ck_assert_int_eq(e->size, origTex.size);

	e = $(loaded, get, (void *) "sounds/weapons/rg_fire.ogg");
	ck_assert_msg(e != NULL, "Missing sounds/weapons/rg_fire.ogg after roundtrip");
	ck_assert_str_eq(e->hash, origSnd.hash);
	ck_assert_int_eq(e->size, origSnd.size);

	e = $(loaded, get, (void *) "maps/edge.nav");
	ck_assert_msg(e != NULL, "Missing maps/edge.nav after roundtrip");
	ck_assert_str_eq(e->hash, origNav.hash);
	ck_assert_int_eq(e->size, origNav.size);

	Manifest_Free(loaded);

} END_TEST

START_TEST(check_Manifest_CheckEntry) {

	// write a file and build a manifest entry from the same content
	const char *content = "test file content";
	write_file("test_asset.tga", content);

	HashTable *manifest = Manifest_Alloc();
	Manifest_AddEntry(manifest, "test_asset.tga", content, strlen(content));

	const ManifestEntry *entry = $(manifest, get, (void *) "test_asset.tga");

	// local file matches — should return true
	ck_assert(Manifest_CheckEntry(entry));

	// overwrite the file with different content
	write_file("test_asset.tga", "different content");

	// now should return false
	ck_assert(!Manifest_CheckEntry(entry));

	Manifest_Free(manifest);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	Test_Init(argc, argv);

	Suite *suite = suite_create("check_manifest");

	{
		TCase *tcase = tcase_create("Manifest_Read");
		tcase_add_checked_fixture(tcase, setup, teardown);
		tcase_add_test(tcase, check_Manifest_Read);
		tcase_add_test(tcase, check_Manifest_Read_empty_lines);
		tcase_add_test(tcase, check_Manifest_Read_missing_file);
		tcase_add_test(tcase, check_Manifest_Parse_respects_len);

		suite_add_tcase(suite, tcase);
	}

	{
		TCase *tcase = tcase_create("Manifest_Write");
		tcase_add_checked_fixture(tcase, setup, teardown);
		tcase_add_test(tcase, check_Manifest_Write);

		suite_add_tcase(suite, tcase);
	}

	{
		TCase *tcase = tcase_create("Manifest_roundtrip");
		tcase_add_checked_fixture(tcase, setup, teardown);
		tcase_add_test(tcase, check_Manifest_roundtrip);

		suite_add_tcase(suite, tcase);
	}

	{
		TCase *tcase = tcase_create("Manifest_CheckEntry");
		tcase_add_checked_fixture(tcase, setup, teardown);
		tcase_add_test(tcase, check_Manifest_CheckEntry);

		suite_add_tcase(suite, tcase);
	}

	int32_t failed = Test_Run(suite);

	Test_Shutdown();
	return failed;
}
