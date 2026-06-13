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

#include "common/demo.h"

quetoo_t quetoo;

/**
 * @brief Setup fixture.
 */
void setup(void) {

  Mem_Init();

  Fs_Init(FS_AUTO_LOAD_ARCHIVES);
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {

  Fs_Shutdown();

  Mem_Shutdown();
}

START_TEST(check_Demo_constants) {
  ck_assert_int_eq(DEMO_FORMAT_VERSION, 2);
  ck_assert_int_eq(DEMO_MAGIC_LEN, 4);
} END_TEST

START_TEST(check_Demo_header_roundtrip) {
  const char *name = "check_demo_header.demo";
  const byte epoch[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

  file_t *w = Fs_OpenWrite(name);
  ck_assert_ptr_nonnull(w);

  const demo_header_t h = {
    .version = DEMO_FORMAT_VERSION, .flags = DEMO_FLAG_CLIENT,
    .protocol_major = 2027, .protocol_minor = 1041, .tick_rate = 40,
    .epoch_len = sizeof(epoch)
  };
  ck_assert(Demo_WriteHeader(w, &h, epoch, sizeof(epoch)));
  ck_assert(Fs_Close(w));

  file_t *r = Fs_OpenRead(name);
  ck_assert_ptr_nonnull(r);
  ck_assert(Demo_IsV2(r));

  demo_header_t h2;
  void *epoch2 = NULL;
  ck_assert(Demo_ReadHeader(r, &h2, &epoch2));
  ck_assert_int_eq(h2.version, h.version);
  ck_assert_int_eq(h2.flags, h.flags);
  ck_assert_int_eq(h2.protocol_major, h.protocol_major);
  ck_assert_int_eq(h2.protocol_minor, h.protocol_minor);
  ck_assert_int_eq(h2.tick_rate, h.tick_rate);
  ck_assert_int_eq(h2.epoch_len, sizeof(epoch));
  ck_assert_mem_eq(epoch2, epoch, sizeof(epoch));

  Mem_Free(epoch2);
  ck_assert(Fs_Close(r));
  Fs_Delete(name);
} END_TEST

START_TEST(check_Demo_not_v2) {
  const char *name = "check_demo_legacy.demo";

  file_t *w = Fs_OpenWrite(name);
  const byte junk[] = { 0xff, 0xff, 0xff, 0xff, 0, 0 };
  Fs_Write(w, junk, 1, sizeof(junk));
  Fs_Close(w);

  file_t *r = Fs_OpenRead(name);
  ck_assert(!Demo_IsV2(r));
  Fs_Close(r);
  Fs_Delete(name);
} END_TEST

START_TEST(check_Demo_record_roundtrip) {
  const char *name = "check_demo_records.demo";
  const byte p0[] = { 10, 11, 12 };
  const byte p1[] = { 20 };

  file_t *w = Fs_OpenWrite(name);
  const demo_header_t h = { .version = 2, .epoch_len = 0 };
  Demo_WriteHeader(w, &h, NULL, 0);
  ck_assert(Demo_WriteRecord(w, DEMO_RECORD_FRAME_KEY, 0, p0, sizeof(p0)));
  ck_assert(Demo_WriteRecord(w, DEMO_RECORD_FRAME_DELTA, 25, p1, sizeof(p1)));
  Fs_Close(w);

  file_t *r = Fs_OpenRead(name);
  demo_header_t h2;
  void *e = NULL;
  Demo_ReadHeader(r, &h2, &e);

  demo_record_t rec;
  byte buf[64];

  ck_assert(Demo_ReadRecord(r, &rec, buf, sizeof(buf)));
  ck_assert_int_eq(rec.type, DEMO_RECORD_FRAME_KEY);
  ck_assert_int_eq(rec.timecode, 0);
  ck_assert_int_eq(rec.length, sizeof(p0));
  ck_assert_mem_eq(buf, p0, sizeof(p0));

  ck_assert(Demo_ReadRecord(r, &rec, buf, sizeof(buf)));
  ck_assert_int_eq(rec.type, DEMO_RECORD_FRAME_DELTA);
  ck_assert_int_eq(rec.timecode, 25);
  ck_assert_int_eq(rec.length, sizeof(p1));
  ck_assert_mem_eq(buf, p1, sizeof(p1));

  ck_assert(!Demo_ReadRecord(r, &rec, buf, sizeof(buf))); // EOF

  Fs_Close(r);
  Fs_Delete(name);
} END_TEST

START_TEST(check_Demo_index_floor) {
  demo_index_entry_t e[3] = { { 0, 100 }, { 2000, 500 }, { 4000, 900 } };
  demo_index_t idx = { .entries = e, .count = 3, .duration = 6000 };

  ck_assert_ptr_eq(Demo_IndexFloor(&idx, 0), &e[0]);
  ck_assert_ptr_eq(Demo_IndexFloor(&idx, 2500), &e[1]);
  ck_assert_ptr_eq(Demo_IndexFloor(&idx, 4000), &e[2]);
  ck_assert_ptr_eq(Demo_IndexFloor(&idx, 99999), &e[2]);
} END_TEST

START_TEST(check_Demo_index_floor_below_first) {
  demo_index_entry_t e[2] = { { 1000, 100 }, { 3000, 500 } };
  demo_index_t idx = { .entries = e, .count = 2 };

  ck_assert_ptr_null(Demo_IndexFloor(&idx, 999));
  ck_assert_ptr_eq(Demo_IndexFloor(&idx, 1000), &e[0]);
} END_TEST

START_TEST(check_Demo_index_roundtrip) {
  const char *name = "check_demo_index.demo";
  const byte p[] = { 7 };

  file_t *w = Fs_OpenWrite(name);
  const demo_header_t h = { .version = 2, .flags = DEMO_FLAG_HAS_INDEX, .epoch_len = 0 };
  Demo_WriteHeader(w, &h, NULL, 0);

  demo_index_entry_t built[2];
  built[0] = (demo_index_entry_t) { .timecode = 0, .offset = (uint64_t) Fs_Tell(w) };
  Demo_WriteRecord(w, DEMO_RECORD_FRAME_KEY, 0, p, sizeof(p));
  Demo_WriteRecord(w, DEMO_RECORD_FRAME_DELTA, 25, p, sizeof(p));
  built[1] = (demo_index_entry_t) { .timecode = 2000, .offset = (uint64_t) Fs_Tell(w) };
  Demo_WriteRecord(w, DEMO_RECORD_FRAME_KEY, 2000, p, sizeof(p));

  const demo_index_t idx = { .entries = built, .count = 2, .duration = 2025 };
  ck_assert(Demo_WriteIndex(w, &idx));
  Fs_Close(w);

  file_t *r = Fs_OpenRead(name);
  demo_index_t got = { 0 };
  ck_assert(Demo_ReadIndex(r, &got));
  ck_assert_int_eq(got.count, 2);
  ck_assert_int_eq(got.duration, 2025);
  ck_assert_int_eq(got.entries[0].timecode, 0);
  ck_assert_int_eq(got.entries[1].timecode, 2000);
  ck_assert_int_eq(got.entries[1].offset, built[1].offset);
  Demo_FreeIndex(&got);

  Fs_Close(r);
  Fs_Delete(name);
} END_TEST

START_TEST(check_Demo_scan_index) {
  const char *name = "check_demo_scan.demo"; // footer-less file
  const byte p[] = { 7 };

  file_t *w = Fs_OpenWrite(name);
  const demo_header_t h = { .version = 2, .epoch_len = 0 };
  Demo_WriteHeader(w, &h, NULL, 0);

  uint64_t off0, off1;
  off0 = (uint64_t) Fs_Tell(w);
  Demo_WriteRecord(w, DEMO_RECORD_FRAME_KEY, 0, p, sizeof(p));
  Demo_WriteRecord(w, DEMO_RECORD_FRAME_DELTA, 25, p, sizeof(p));
  off1 = (uint64_t) Fs_Tell(w);
  Demo_WriteRecord(w, DEMO_RECORD_FRAME_KEY, 2000, p, sizeof(p));
  Fs_Close(w); // no index footer

  file_t *r = Fs_OpenRead(name);
  demo_header_t h2;
  void *e = NULL;
  Demo_ReadHeader(r, &h2, &e); // leaves cursor at first record

  demo_index_t scanned = { 0 };
  ck_assert(Demo_ScanIndex(r, &scanned));
  ck_assert_int_eq(scanned.count, 2);
  ck_assert_int_eq(scanned.duration, 2000);
  ck_assert_int_eq(scanned.entries[0].timecode, 0);
  ck_assert_int_eq(scanned.entries[0].offset, off0);
  ck_assert_int_eq(scanned.entries[1].timecode, 2000);
  ck_assert_int_eq(scanned.entries[1].offset, off1);
  Demo_FreeIndex(&scanned);

  Fs_Close(r);
  Fs_Delete(name);
} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  TCase *tcase = tcase_create("check_demo");
  tcase_add_checked_fixture(tcase, setup, teardown);

  tcase_add_test(tcase, check_Demo_constants);
  tcase_add_test(tcase, check_Demo_header_roundtrip);
  tcase_add_test(tcase, check_Demo_not_v2);
  tcase_add_test(tcase, check_Demo_record_roundtrip);
  tcase_add_test(tcase, check_Demo_index_floor);
  tcase_add_test(tcase, check_Demo_index_floor_below_first);
  tcase_add_test(tcase, check_Demo_index_roundtrip);
  tcase_add_test(tcase, check_Demo_scan_index);

  Suite *suite = suite_create("check_demo");
  suite_add_tcase(suite, tcase);

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
