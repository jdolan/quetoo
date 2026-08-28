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

#include <time.h>

#include <Objectively/List.h>

#include "g_race.h"

/**
 * @file
 * @brief The records: one personal best per client per movement, in a text
 * file per map that reads like `maps.lst`:
 *
 *     {
 *       "guid" "..."
 *       "name" "..."
 *       "movement" "race"
 *       "time" "83412"
 *       ...
 *     }
 *
 * The file is read whole when the level is configured and rewritten whole when
 * a best is beaten; it is never sent anywhere, so it is written to be read by a
 * person. A block that cannot be a record is warned about and skipped, as a
 * `maps.lst` entry without a name is.
 */

// how many of the best are published for the scoreboard
#define RACE_RECORDS_SHOWN 15

static const char *G_Race_RecordsPath(void) {
  return va("records/%s.rec", g_level.name);
}

/**
 * @brief Formats a time as the file and the messages show it.
 */
static const char *G_Race_RecordTime(uint32_t ms) {
  return va("%u:%02u.%03u", ms / 60000, ms / 1000 % 60, ms % 1000);
}

/**
 * @brief FNV-1a over the parameters a run was made under: not a key, just a way
 * to tell two records under the same movement apart if its cvars were changed.
 */
static uint32_t G_Race_HashBytes(uint32_t hash, const void *data, size_t length) {

  const uint8_t *bytes = data;

  for (size_t i = 0; i < length; i++) {
    hash = (hash ^ bytes[i]) * 16777619u;
  }

  return hash;
}

static uint32_t G_Race_HashParams(const pm_params_t *params) {

  uint32_t hash = 2166136261u;

  // the fields, not the struct: there is padding after movement
  hash = G_Race_HashBytes(hash, &params->gravity, sizeof(params->gravity));
  hash = G_Race_HashBytes(hash, &params->movement, sizeof(params->movement));
  hash = G_Race_HashBytes(hash, &params->accel_ground, sizeof(*params) - offsetof(pm_params_t, accel_ground));

  return hash;
}

/**
 * @brief Reads `key_N` for N from `first` up, as long as they are there: a
 * value is at most `MAX_BSP_ENTITY_VALUE` long, so a list is one key per time.
 */
static uint16_t G_Race_ParseTimes(const cm_entity_t *def, const char *key, int32_t first, uint32_t *times) {
  uint16_t count = 0;

  for (int32_t n = first; n <= RACE_MAX_CHECKPOINTS; n++) {
    const cm_entity_t *time = gi.EntityValue(def, va("%s_%d", key, n));

    if (!(time->parsed & ENTITY_INTEGER)) {
      break;
    }

    times[count++] = time->integer;
  }

  return count;
}

static int32_t G_Race_CompareRecords(const void *a, const void *b) {
  const g_race_record_t *ra = a, *rb = b;

  if (ra->movement != rb->movement) {
    return (int32_t) ra->movement - (int32_t) rb->movement;
  }

  return ra->time < rb->time ? -1 : ra->time > rb->time ? 1 : 0;
}

static void G_Race_SortRecords(void) {
  qsort(g_level.race_records, g_level.race_record_count, sizeof(g_race_record_t), G_Race_CompareRecords);
}

/**
 * @brief Room for one more record, in level memory so that the level's end
 * frees it along with everything else.
 */
static g_race_record_t *G_Race_AddRecord(void) {

  if (g_level.race_record_count == g_level.race_record_capacity) {
    const size_t capacity = g_level.race_record_capacity ? g_level.race_record_capacity * 2 : 32;
    g_race_record_t *records = gi.Malloc(capacity * sizeof(g_race_record_t), MEM_TAG_GAME_LEVEL);

    if (g_level.race_records) {
      memcpy(records, g_level.race_records, g_level.race_record_count * sizeof(g_race_record_t));
      gi.Free(g_level.race_records);
    }

    g_level.race_records = records;
    g_level.race_record_capacity = capacity;
  }

  g_race_record_t *record = &g_level.race_records[g_level.race_record_count++];
  memset(record, 0, sizeof(*record));
  return record;
}

static g_race_record_t *G_Race_FindRecord(const char *guid, pm_movement_t movement) {

  for (size_t i = 0; i < g_level.race_record_count; i++) {
    g_race_record_t *record = &g_level.race_records[i];

    if (record->movement == movement && !q_strcmp(record->guid, guid)) {
      return record;
    }
  }

  return NULL;
}

const g_race_record_t *G_Race_Record(const char *guid, pm_movement_t movement) {
  return G_Race_FindRecord(guid, movement);
}

size_t G_Race_Rank(const g_race_record_t *record, size_t *count) {
  size_t rank = 0;

  *count = 0;
  for (size_t i = 0; i < g_level.race_record_count; i++) {
    const g_race_record_t *r = &g_level.race_records[i];

    if (r->movement != record->movement) {
      continue;
    }

    (*count)++;
    if (r == record) {
      rank = *count;
    }
  }

  return rank;
}

/**
 * @brief The best under the level's movement, as `name\time` pairs, as many as
 * are shown and fit.
 */
static void G_Race_PublishRecords(void) {
  char string[MAX_STRING_CHARS] = "";
  size_t shown = 0;

  for (size_t i = 0; i < g_level.race_record_count && shown < RACE_RECORDS_SHOWN; i++) {
    const g_race_record_t *record = &g_level.race_records[i];

    if (record->movement != g_level.movement) {
      continue;
    }

    const char *pair = va("%s%s\\%u", shown ? "\\" : "", record->name, record->time);

    if (q_strlen(string) + q_strlen(pair) >= sizeof(string)) {
      break;
    }

    q_strlcat(string, pair, sizeof(string));
    shown++;
  }

  gi.SetConfigString(CS_RACE_RECORDS, string);
}

/**
 * @brief Reads one block, or says why it is not a record.
 */
static bool G_Race_ParseRecord(const cm_entity_t *def, int32_t index) {

  const char *guid = gi.EntityValue(def, "guid")->nullable_string;
  const char *movement_name = gi.EntityValue(def, "movement")->nullable_string;
  const cm_entity_t *time = gi.EntityValue(def, "time");

  pm_movement_t movement;

  if (!guid || !*guid) {
    G_Warn("Record %d in %s has no guid\n", index, G_Race_RecordsPath());
    return false;
  }

  if (!movement_name || !Pm_MovementByName(movement_name, &movement)) {
    G_Warn("Record %d in %s has no movement, or one nothing answers to\n", index, G_Race_RecordsPath());
    return false;
  }

  if (!(time->parsed & ENTITY_INTEGER) || time->integer <= 0) {
    G_Warn("Record %d in %s has no time\n", index, G_Race_RecordsPath());
    return false;
  }

  if (G_Race_FindRecord(guid, movement)) {
    G_Warn("Record %d in %s repeats %s under %s\n", index, G_Race_RecordsPath(), guid, movement_name);
    return false;
  }

  g_race_record_t *record = G_Race_AddRecord();

  q_strlcpy(record->guid, guid, sizeof(record->guid));
  q_strlcpy(record->name, gi.EntityValue(def, "name")->string, sizeof(record->name));
  q_strlcpy(record->ip, gi.EntityValue(def, "ip")->string, sizeof(record->ip));
  q_strlcpy(record->date, gi.EntityValue(def, "date")->string, sizeof(record->date));

  record->movement = movement;
  record->params = (uint32_t) strtoul(gi.EntityValue(def, "params")->string, NULL, 16);
  record->time = time->integer;

  record->checkpoint_count = G_Race_ParseTimes(def, "checkpoint", 1, record->checkpoint_times);
  record->split_count = G_Race_ParseTimes(def, "split", 1, record->split_times);
  record->stage_count = G_Race_ParseTimes(def, "stage", 2, record->stage_times);

  sscanf(gi.EntityValue(def, "speed")->string, "%f %f %f",
         &record->start_speed, &record->top_speed, &record->average_speed);

  return true;
}

void G_Race_LoadRecords(void) {

  // the level's end freed the last map's, and this may be the same map again
  g_level.race_records = NULL;
  g_level.race_record_count = g_level.race_record_capacity = 0;

  void *buffer;
  if (gi.LoadFile(G_Race_RecordsPath(), &buffer) <= 0) { // missing, or empty and so not even a buffer
    G_Debug("No records at %s\n", G_Race_RecordsPath());
    G_Race_PublishRecords();
    return;
  }

  List *defs = gi.LoadEntities(buffer);

  int32_t index = 0;
  for (const ListNode *node = defs->head; node; node = node->next, index++) {
    cm_entity_t *def = node->element;
    G_Race_ParseRecord(def, index);
    gi.FreeEntity(def);
  }

  release(defs);
  gi.FreeFile(buffer);

  G_Race_SortRecords();
  G_Race_PublishRecords();

  G_Debug("Loaded %zu records from %s\n", g_level.race_record_count, G_Race_RecordsPath());
}

static void G_Race_WriteLine(file_t *file, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void G_Race_WriteLine(file_t *file, const char *fmt, ...) {
  char line[MAX_STRING_CHARS];

  va_list args;
  va_start(args, fmt);
  const int32_t len = vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);

  gi.WriteFile(file, line, 1, Mini(len, (int32_t) sizeof(line) - 1));
}

static void G_Race_WriteTimes(file_t *file, const char *key, int32_t first, const uint32_t *times, uint16_t count) {

  for (uint16_t i = 0; i < count; i++) {
    G_Race_WriteLine(file, "  \"%s_%d\" \"%u\"\n", key, first + i, times[i]);
  }
}

static void G_Race_SaveRecords(void) {

  file_t *file = gi.OpenFileWrite(G_Race_RecordsPath());
  if (!file) {
    G_Warn("Failed to open %s for writing\n", G_Race_RecordsPath());
    return;
  }

  G_Race_WriteLine(file, "// Race records for %s: one personal best per player per movement\n", g_level.name);

  for (size_t i = 0; i < g_level.race_record_count; i++) {
    const g_race_record_t *r = &g_level.race_records[i];

    G_Race_WriteLine(file, "{\n");
    G_Race_WriteLine(file, "  \"guid\" \"%s\"\n", r->guid);
    G_Race_WriteLine(file, "  \"name\" \"%s\"\n", r->name);
    G_Race_WriteLine(file, "  \"ip\" \"%s\"\n", r->ip);
    G_Race_WriteLine(file, "  \"date\" \"%s\"\n", r->date);
    G_Race_WriteLine(file, "  \"movement\" \"%s\"\n", Pm_Movement(r->movement)->name);
    G_Race_WriteLine(file, "  \"params\" \"%08x\"\n", r->params);
    G_Race_WriteLine(file, "  \"time\" \"%u\"\n", r->time);
    G_Race_WriteTimes(file, "checkpoint", 1, r->checkpoint_times, r->checkpoint_count);
    G_Race_WriteTimes(file, "split", 1, r->split_times, r->split_count);
    G_Race_WriteTimes(file, "stage", 2, r->stage_times, r->stage_count);
    G_Race_WriteLine(file, "  \"speed\" \"%.0f %.0f %.0f\"\n", r->start_speed, r->top_speed, r->average_speed);
    G_Race_WriteLine(file, "}\n");
  }

  gi.CloseFile(file);
}

bool G_Race_SubmitRecord(g_client_t *cl) {
  const g_race_run_t *run = &cl->race_run;

  const g_race_record_t *best = NULL;
  for (size_t i = 0; i < g_level.race_record_count; i++) {
    if (g_level.race_records[i].movement == run->movement) {
      best = &g_level.race_records[i];
      break;
    }
  }

  g_race_record_t *record = G_Race_FindRecord(cl->persistent.guid, run->movement);

  if (record && record->time <= run->elapsed) {
    gi.ClientPrint(cl, PRINT_HIGH, "Your best is %s (+%s)\n", G_Race_RecordTime(record->time),
                   G_Race_RecordTime(run->elapsed - record->time));
    return false;
  }

  const uint32_t previous = record ? record->time : 0;
  const uint32_t best_time = best ? best->time : 0;

  if (!record) {
    record = G_Race_AddRecord();
  }

  q_strlcpy(record->guid, cl->persistent.guid, sizeof(record->guid));
  q_strlcpy(record->name, cl->persistent.net_name, sizeof(record->name));
  q_strlcpy(record->ip, InfoString_Get(cl->persistent.user_info, "ip"), sizeof(record->ip));

  const time_t now = time(NULL);
  strftime(record->date, sizeof(record->date), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

  record->movement = run->movement;
  record->params = G_Race_HashParams(&cl->ps.pm_state.params);
  record->time = run->elapsed;

  record->checkpoint_count = run->checkpoint_count;
  memcpy(record->checkpoint_times, run->checkpoint_times, sizeof(record->checkpoint_times));
  record->split_count = run->split_count;
  memcpy(record->split_times, run->split_times, sizeof(record->split_times));
  record->stage_count = run->stage > 1 ? run->stage - 1 : 0;
  memcpy(record->stage_times, run->stage_times, sizeof(record->stage_times));

  record->start_speed = run->start_speed;
  record->top_speed = run->top_speed;
  record->average_speed = run->speed_samples ? run->speed_sum / run->speed_samples : 0.f;

  G_Race_SortRecords();
  G_Race_SaveRecords();
  G_Race_PublishRecords();

  const bool course_record = !best_time || run->elapsed < best_time;

  if (course_record) {
    if (best_time) {
      gi.BroadcastPrint(PRINT_HIGH, "^3%s set the course record, %s faster^7\n",
                        cl->persistent.net_name, G_Race_RecordTime(best_time - run->elapsed));
    } else {
      gi.BroadcastPrint(PRINT_HIGH, "^3%s set the first course record^7\n", cl->persistent.net_name);
    }
  } else if (previous) {
    gi.ClientPrint(cl, PRINT_HIGH, "^2Personal best^7, %s faster\n", G_Race_RecordTime(previous - run->elapsed));
  } else {
    gi.ClientPrint(cl, PRINT_HIGH, "^2Personal best^7\n");
  }

  size_t count;
  const size_t rank = G_Race_Rank(G_Race_FindRecord(cl->persistent.guid, run->movement), &count);
  gi.ClientPrint(cl, PRINT_HIGH, "You are #%zu of %zu\n", rank, count);

  return course_record;
}
