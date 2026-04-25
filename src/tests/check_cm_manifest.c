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
#include "collision/cm_manifest.h"

quetoo_t quetoo;

/**
 * @brief Setup fixture.
 */
void setup(void) {
	Mem_Init();
	Fs_Init(FS_NONE);
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
	file_t *file = Fs_OpenWrite(path);
	ck_assert_msg(file != NULL, "Failed to open %s for writing", path);
	Fs_Print(file, "%s", content);
	Fs_Close(file);
}

START_TEST(check_Cm_ReadManifest) {

	write_file("test.mf",
		"d41d8cd98f00b204e9800998ecf8427e 1234 textures/edge/floor01_d.tga\n"
		"a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6 5678 sounds/weapons/rg_fire.ogg\n"
	);

	GList *manifest = Cm_ReadManifest("test.mf");
	ck_assert_msg(manifest != NULL, "Cm_ReadManifest returned NULL");
	ck_assert_int_eq(g_list_length(manifest), 2);

	const cm_manifest_entry_t *e0 = (const cm_manifest_entry_t *) manifest->data;
	ck_assert_str_eq(e0->hash, "d41d8cd98f00b204e9800998ecf8427e");
	ck_assert_int_eq(e0->size, 1234);
	ck_assert_str_eq(e0->path, "textures/edge/floor01_d.tga");

	const cm_manifest_entry_t *e1 = (const cm_manifest_entry_t *) manifest->next->data;
	ck_assert_str_eq(e1->hash, "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6");
	ck_assert_int_eq(e1->size, 5678);
	ck_assert_str_eq(e1->path, "sounds/weapons/rg_fire.ogg");

	Cm_FreeManifest(manifest);

} END_TEST

START_TEST(check_Cm_ReadManifest_empty_lines) {

	write_file("test_empty.mf",
		"\n"
		"d41d8cd98f00b204e9800998ecf8427e 100 textures/foo.tga\n"
		"\n"
		"\n"
		"a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6 200 sounds/bar.ogg\n"
		"\n"
	);

	GList *manifest = Cm_ReadManifest("test_empty.mf");
	ck_assert_msg(manifest != NULL, "Cm_ReadManifest returned NULL");
	ck_assert_int_eq(g_list_length(manifest), 2);

	Cm_FreeManifest(manifest);

} END_TEST

START_TEST(check_Cm_ReadManifest_missing_file) {

	GList *manifest = Cm_ReadManifest("nonexistent.mf");
	ck_assert_msg(manifest == NULL, "Expected NULL for missing manifest");

} END_TEST

START_TEST(check_Cm_WriteManifest) {

	const char *content1 = "hello";
	const char *content2 = "world";

	GList *manifest = NULL;
	Cm_AddManifestEntry(&manifest, "textures/edge/floor01_d.tga", content1, strlen(content1));
	Cm_AddManifestEntry(&manifest, "sounds/weapons/rg_fire.ogg", content2, strlen(content2));

	const int32_t count = Cm_WriteManifest("test_write.mf", manifest);
	ck_assert_int_eq(count, 2);

	Cm_FreeManifest(manifest);

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

START_TEST(check_Cm_Manifest_roundtrip) {

	const char *content1 = "file1data";
	const char *content2 = "file2data";
	const char *content3 = "file3data";

	GList *manifest = NULL;
	Cm_AddManifestEntry(&manifest, "textures/edge/floor01_d.tga", content1, strlen(content1));
	Cm_AddManifestEntry(&manifest, "sounds/weapons/rg_fire.ogg", content2, strlen(content2));
	Cm_AddManifestEntry(&manifest, "maps/edge.nav", content3, strlen(content3));

	// save original hashes for comparison
	cm_manifest_entry_t orig[3];
	GList *e = manifest;
	for (int32_t i = 0; i < 3; i++, e = e->next) {
		orig[i] = *(const cm_manifest_entry_t *) e->data;
	}

	Cm_WriteManifest("test_roundtrip.mf", manifest);
	Cm_FreeManifest(manifest);

	GList *loaded = Cm_ReadManifest("test_roundtrip.mf");
	ck_assert_msg(loaded != NULL, "Cm_ReadManifest returned NULL after write");
	ck_assert_int_eq(g_list_length(loaded), 3);

	e = loaded;
	for (int32_t i = 0; i < 3; i++, e = e->next) {
		const cm_manifest_entry_t *entry = (const cm_manifest_entry_t *) e->data;
		ck_assert_str_eq(entry->hash, orig[i].hash);
		ck_assert_int_eq(entry->size, orig[i].size);
		ck_assert_str_eq(entry->path, orig[i].path);
	}

	Cm_FreeManifest(loaded);

} END_TEST

START_TEST(check_Cm_CheckManifestEntry) {

	// write a file and build a manifest entry from the same content
	const char *content = "test file content";
	write_file("test_asset.tga", content);

	GList *manifest = NULL;
	Cm_AddManifestEntry(&manifest, "test_asset.tga", content, strlen(content));

	const cm_manifest_entry_t *entry = (const cm_manifest_entry_t *) manifest->data;

	// local file matches — should return true
	ck_assert(Cm_CheckManifestEntry(entry));

	// overwrite the file with different content
	write_file("test_asset.tga", "different content");

	// now should return false
	ck_assert(!Cm_CheckManifestEntry(entry));

	Cm_FreeManifest(manifest);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	Test_Init(argc, argv);

	Suite *suite = suite_create("check_cm_manifest");

	{
		TCase *tcase = tcase_create("Cm_ReadManifest");
		tcase_add_checked_fixture(tcase, setup, teardown);
		tcase_add_test(tcase, check_Cm_ReadManifest);
		tcase_add_test(tcase, check_Cm_ReadManifest_empty_lines);
		tcase_add_test(tcase, check_Cm_ReadManifest_missing_file);

		suite_add_tcase(suite, tcase);
	}

	{
		TCase *tcase = tcase_create("Cm_WriteManifest");
		tcase_add_checked_fixture(tcase, setup, teardown);
		tcase_add_test(tcase, check_Cm_WriteManifest);

		suite_add_tcase(suite, tcase);
	}

	{
		TCase *tcase = tcase_create("Cm_Manifest_roundtrip");
		tcase_add_checked_fixture(tcase, setup, teardown);
		tcase_add_test(tcase, check_Cm_Manifest_roundtrip);

		suite_add_tcase(suite, tcase);
	}

	{
		TCase *tcase = tcase_create("Cm_CheckManifestEntry");
		tcase_add_checked_fixture(tcase, setup, teardown);
		tcase_add_test(tcase, check_Cm_CheckManifestEntry);

		suite_add_tcase(suite, tcase);
	}

	int32_t failed = Test_Run(suite);

	Test_Shutdown();
	return failed;
}
