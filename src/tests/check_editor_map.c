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
#include "collision/cm_entity.h"

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

// ============================================================================
// Test: Simple entity with one brush
// ============================================================================

START_TEST(check_parse_simple_brush) {

  const char *mapText =
    "{\n"
    "\"classname\" \"worldspawn\"\n"
    "{\n"
    "( 0 0 0 ) ( 1 0 0 ) ( 0 1 0 ) test 0 0 0 0.5 0.5 0 0 0\n"
    "( 0 0 0 ) ( 0 0 1 ) ( 1 0 0 ) test 0 0 0 0.5 0.5 0 0 0\n"
    "( 0 0 0 ) ( 0 1 0 ) ( 0 0 1 ) test 0 0 0 0.5 0.5 0 0 0\n"
    "}\n"
    "}\n";

  CmEntity *e = Cm_AllocEntity();
  SDL_strlcpy(e->key, "classname", sizeof(e->key));
  SDL_strlcpy(e->string, "worldspawn", sizeof(e->string));

  CmEntity *entities[] = { e };
  Cm_ParseMapBrushes(mapText, entities, 1);

  ck_assert_ptr_nonnull(e->brushes);
  ck_assert(strstr(e->brushes, "( 0 0 0 ) ( 1 0 0 ) ( 0 1 0 ) test") != NULL);
  ck_assert(strstr(e->brushes, "( 0 0 0 ) ( 0 0 1 ) ( 1 0 0 ) test") != NULL);

  Mem_FreeTag(MEM_TAG_COLLISION);

} END_TEST

// ============================================================================
// Test: Entity without brushes
// ============================================================================

START_TEST(check_parse_no_brushes) {

  const char *mapText =
    "{\n"
    "\"classname\" \"info_player_deathmatch\"\n"
    "\"origin\" \"0 0 0\"\n"
    "}\n";

  CmEntity *e = Cm_AllocEntity();
  SDL_strlcpy(e->key, "classname", sizeof(e->key));
  SDL_strlcpy(e->string, "info_player_deathmatch", sizeof(e->string));

  CmEntity *entities[] = { e };
  Cm_ParseMapBrushes(mapText, entities, 1);

  ck_assert_ptr_null(e->brushes);

  Mem_FreeTag(MEM_TAG_COLLISION);

} END_TEST

// ============================================================================
// Test: Entity with patchDef2
// ============================================================================

START_TEST(check_parse_patchdef2) {

  const char *mapText =
    "{\n"
    "\"classname\" \"worldspawn\"\n"
    "{\n"
    "  patchDef2\n"
    "  {\n"
    "   textures/test\n"
    "   ( 3 3 0 0 0 )\n"
    "(\n"
    "( ( 0 0 0 0 0 ) ( 64 0 0 0.5 0 ) ( 128 0 0 1 0 ) )\n"
    "( ( 0 64 0 0 0.5 ) ( 64 64 32 0.5 0.5 ) ( 128 64 0 1 0.5 ) )\n"
    "( ( 0 128 0 0 1 ) ( 64 128 0 0.5 1 ) ( 128 128 0 1 1 ) )\n"
    ")\n"
    "  }\n"
    "}\n"
    "}\n";

  CmEntity *e = Cm_AllocEntity();
  SDL_strlcpy(e->key, "classname", sizeof(e->key));
  SDL_strlcpy(e->string, "worldspawn", sizeof(e->string));

  CmEntity *entities[] = { e };
  Cm_ParseMapBrushes(mapText, entities, 1);

  ck_assert_ptr_nonnull(e->brushes);
  ck_assert(strstr(e->brushes, "patchDef2") != NULL);
  ck_assert(strstr(e->brushes, "textures/test") != NULL);
  ck_assert(strstr(e->brushes, "64 64 32 0.5 0.5") != NULL);

  Mem_FreeTag(MEM_TAG_COLLISION);

} END_TEST

// ============================================================================
// Test: Mixed brushes and patches in one entity
// ============================================================================

START_TEST(check_parse_mixed_brush_and_patch) {

  const char *mapText =
    "{\n"
    "\"classname\" \"worldspawn\"\n"
    "// brush 0\n"
    "{\n"
    "( 0 0 0 ) ( 1 0 0 ) ( 0 1 0 ) brick 0 0 0 0.5 0.5 0 0 0\n"
    "( 0 0 0 ) ( 0 0 1 ) ( 1 0 0 ) brick 0 0 0 0.5 0.5 0 0 0\n"
    "}\n"
    "// brush 1\n"
    "{\n"
    "  patchDef2\n"
    "  {\n"
    "   metal\n"
    "   ( 3 3 0 0 0 )\n"
    "(\n"
    "( ( 0 0 0 0 0 ) ( 64 0 0 0.5 0 ) ( 128 0 0 1 0 ) )\n"
    "( ( 0 64 0 0 0.5 ) ( 64 64 32 0.5 0.5 ) ( 128 64 0 1 0.5 ) )\n"
    "( ( 0 128 0 0 1 ) ( 64 128 0 0.5 1 ) ( 128 128 0 1 1 ) )\n"
    ")\n"
    "  }\n"
    "}\n"
    "}\n";

  CmEntity *e = Cm_AllocEntity();
  SDL_strlcpy(e->key, "classname", sizeof(e->key));
  SDL_strlcpy(e->string, "worldspawn", sizeof(e->string));

  CmEntity *entities[] = { e };
  Cm_ParseMapBrushes(mapText, entities, 1);

  ck_assert_ptr_nonnull(e->brushes);
  ck_assert(strstr(e->brushes, "brick") != NULL);
  ck_assert(strstr(e->brushes, "patchDef2") != NULL);
  ck_assert(strstr(e->brushes, "metal") != NULL);

  Mem_FreeTag(MEM_TAG_COLLISION);

} END_TEST

// ============================================================================
// Test: Multiple entities with correct brush assignment
// ============================================================================

START_TEST(check_parse_multiple_entities) {

  const char *mapText =
    "// entity 0\n"
    "{\n"
    "\"classname\" \"worldspawn\"\n"
    "{\n"
    "( 0 0 0 ) ( 1 0 0 ) ( 0 1 0 ) floor 0 0 0 0.5 0.5 0 0 0\n"
    "}\n"
    "}\n"
    "// entity 1\n"
    "{\n"
    "\"classname\" \"info_player_deathmatch\"\n"
    "\"origin\" \"0 0 0\"\n"
    "}\n"
    "// entity 2\n"
    "{\n"
    "\"classname\" \"func_plat\"\n"
    "{\n"
    "( 0 0 0 ) ( 1 0 0 ) ( 0 1 0 ) plat_tex 0 0 0 0.5 0.5 0 0 0\n"
    "}\n"
    "}\n";

  CmEntity *e0 = Cm_AllocEntity();
  CmEntity *e1 = Cm_AllocEntity();
  CmEntity *e2 = Cm_AllocEntity();

  CmEntity *entities[] = { e0, e1, e2 };
  Cm_ParseMapBrushes(mapText, entities, 3);

  // worldspawn gets its brush
  ck_assert_ptr_nonnull(e0->brushes);
  ck_assert(strstr(e0->brushes, "floor") != NULL);

  // info_player_deathmatch has no brushes
  ck_assert_ptr_null(e1->brushes);

  // func_plat gets its brush
  ck_assert_ptr_nonnull(e2->brushes);
  ck_assert(strstr(e2->brushes, "plat_tex") != NULL);

  // Verify no cross-contamination
  ck_assert(strstr(e0->brushes, "plat_tex") == NULL);
  ck_assert(strstr(e2->brushes, "floor") == NULL);

  Mem_FreeTag(MEM_TAG_COLLISION);

} END_TEST

// ============================================================================
// Test: Long key-value exceeding MAX_TOKEN_CHARS (the license string bug)
// ============================================================================

START_TEST(check_parse_long_token_value) {

  // Build a value string longer than MAX_TOKEN_CHARS (512)
  char longValue[700];
  memset(longValue, 'x', sizeof(longValue) - 1);
  longValue[sizeof(longValue) - 1] = '\0';

  char mapText[2048];
  snprintf(mapText, sizeof(mapText),
    "{\n"
    "\"classname\" \"worldspawn\"\n"
    "\"license\" \"%s\"\n"
    "{\n"
    "( 0 0 0 ) ( 1 0 0 ) ( 0 1 0 ) brick 0 0 0 0.5 0.5 0 0 0\n"
    "}\n"
    "}\n"
    "{\n"
    "\"classname\" \"light\"\n"
    "\"origin\" \"0 0 0\"\n"
    "}\n",
    longValue);

  CmEntity *e0 = Cm_AllocEntity();
  CmEntity *e1 = Cm_AllocEntity();

  CmEntity *entities[] = { e0, e1 };
  Cm_ParseMapBrushes(mapText, entities, 2);

  // worldspawn must still get its brushes despite the long token
  ck_assert_ptr_nonnull(e0->brushes);
  ck_assert(strstr(e0->brushes, "brick") != NULL);

  // light must NOT get worldspawn's brushes
  ck_assert_ptr_null(e1->brushes);

  Mem_FreeTag(MEM_TAG_COLLISION);

} END_TEST

// ============================================================================
// Test: Comments between brushes are preserved in captured text
// ============================================================================

START_TEST(check_parse_preserves_comments) {

  const char *mapText =
    "{\n"
    "\"classname\" \"worldspawn\"\n"
    "// brush 0\n"
    "{\n"
    "( 0 0 0 ) ( 1 0 0 ) ( 0 1 0 ) floor 0 0 0 0.5 0.5 0 0 0\n"
    "}\n"
    "// brush 1\n"
    "{\n"
    "( 0 0 0 ) ( 0 0 1 ) ( 1 0 0 ) wall 0 0 0 0.5 0.5 0 0 0\n"
    "}\n"
    "}\n";

  CmEntity *e = Cm_AllocEntity();
  SDL_strlcpy(e->key, "classname", sizeof(e->key));
  SDL_strlcpy(e->string, "worldspawn", sizeof(e->string));

  CmEntity *entities[] = { e };
  Cm_ParseMapBrushes(mapText, entities, 1);

  ck_assert_ptr_nonnull(e->brushes);
  // Both brushes must be captured
  ck_assert(strstr(e->brushes, "floor") != NULL);
  ck_assert(strstr(e->brushes, "wall") != NULL);
  // The comment between brushes must be preserved in the raw text
  ck_assert(strstr(e->brushes, "// brush 1") != NULL);

  Mem_FreeTag(MEM_TAG_COLLISION);

} END_TEST

// ============================================================================
// Test: CopyEntity preserves brushes through sort
// ============================================================================

START_TEST(check_copy_entity_preserves_brushes) {

  CmEntity *origin = Cm_AllocEntity();
  SDL_strlcpy(origin->key, "origin", sizeof(origin->key));
  SDL_strlcpy(origin->string, "128 256 512", sizeof(origin->string));

  CmEntity *classname = Cm_AllocEntity();
  SDL_strlcpy(classname->key, "classname", sizeof(classname->key));
  SDL_strlcpy(classname->string, "worldspawn", sizeof(classname->string));
  classname->brushes = Mem_TagCopyString("{ test brush data }\n", MEM_TAG_COLLISION);

  // Build a linked list: origin -> classname (brushes on classname node)
  origin->next = classname;
  classname->prev = origin;

  CmEntity *copy = Cm_CopyEntity(origin);

  // After copy+sort, brushes must be findable somewhere in the linked list
  const char *foundBrushes = NULL;
  for (const CmEntity *n = copy; n; n = n->next) {
    if (n->brushes) {
      foundBrushes = n->brushes;
      break;
    }
  }

  ck_assert_ptr_nonnull(foundBrushes);
  ck_assert(strstr(foundBrushes, "test brush data") != NULL);

  Cm_FreeEntity(copy);
  Mem_FreeTag(MEM_TAG_COLLISION);

} END_TEST

// ============================================================================
// Test: Fewer BSP entities than map entities (func_group merged)
// ============================================================================

START_TEST(check_parse_fewer_bsp_entities) {

  // Map has 3 entities, but BSP only has 2 (func_group merged into worldspawn)
  const char *mapText =
    "{\n"
    "\"classname\" \"worldspawn\"\n"
    "{\n"
    "( 0 0 0 ) ( 1 0 0 ) ( 0 1 0 ) floor 0 0 0 0.5 0.5 0 0 0\n"
    "}\n"
    "}\n"
    "{\n"
    "\"classname\" \"light\"\n"
    "\"origin\" \"0 0 0\"\n"
    "}\n"
    "{\n"
    "\"classname\" \"func_group\"\n"
    "{\n"
    "( 0 0 0 ) ( 1 0 0 ) ( 0 1 0 ) detail 0 0 0 0.5 0.5 0 0 0\n"
    "}\n"
    "}\n";

  CmEntity *e0 = Cm_AllocEntity();
  CmEntity *e1 = Cm_AllocEntity();

  // BSP only has worldspawn and light (func_group was merged)
  CmEntity *entities[] = { e0, e1 };
  Cm_ParseMapBrushes(mapText, entities, 2);

  // worldspawn gets its brush
  ck_assert_ptr_nonnull(e0->brushes);
  ck_assert(strstr(e0->brushes, "floor") != NULL);

  // light should have no brushes (not func_group's brushes!)
  ck_assert_ptr_null(e1->brushes);

  Mem_FreeTag(MEM_TAG_COLLISION);

} END_TEST

// ============================================================================
// Test entry point
// ============================================================================

int32_t main(int32_t argc, char **argv) {

  Test_Init(argc, argv);

  Suite *suite = suite_create("check_editor_map");

  {
    TCase *tcase = tcase_create("ParseMapBrushes");
    tcase_add_checked_fixture(tcase, setup, teardown);

    tcase_add_test(tcase, check_parse_simple_brush);
    tcase_add_test(tcase, check_parse_no_brushes);
    tcase_add_test(tcase, check_parse_patchdef2);
    tcase_add_test(tcase, check_parse_mixed_brush_and_patch);
    tcase_add_test(tcase, check_parse_multiple_entities);
    tcase_add_test(tcase, check_parse_long_token_value);
    tcase_add_test(tcase, check_parse_preserves_comments);
    tcase_add_test(tcase, check_parse_fewer_bsp_entities);

    suite_add_tcase(suite, tcase);
  }

  {
    TCase *tcase = tcase_create("CopyEntityBrushes");
    tcase_add_checked_fixture(tcase, setup, teardown);

    tcase_add_test(tcase, check_copy_entity_preserves_brushes);

    suite_add_tcase(suite, tcase);
  }

  int32_t failed = Test_Run(suite);

  Test_Shutdown();
  return failed;
}
