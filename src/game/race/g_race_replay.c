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

#include "g_race.h"

/**
 * @file
 * @brief The raceline and the ghost. Every run in race mode is sampled as it
 * goes, one sample a tick; a run that sets the course record keeps its line in
 * `records/<map>-<movement>.ghost`, and every other line is dropped with its
 * run. The file is text, a header of `key value` lines and then one sample a
 * line, so that it can be read by a person as the records can.
 *
 * A run's samples live here rather than on the client, which is cleared whole
 * on every respawn: a run that ends in one would otherwise take its buffer
 * with it.
 *
 * The ghost is a plain entity dressed as a player and driven from the course
 * record's samples, spawned when a client who asked for one starts a run, so
 * that the two race side by side. It is tied to the BSP the record was set on:
 * a rebuilt map may have moved the walls the ghost runs along.
 */

// how the ghost is dressed until the client game reads CS_RACE_GHOST: as the
// viewer, since a player model needs a client slot for its skin, and opaque,
// since EF_DESPAWN fades from a timestamp a new entity never gets
#define RACE_GHOST_EFFECTS (EF_CLIENT | EF_RACE_GHOST)

static g_race_line_t g_race_lines[MAX_CLIENTS];

static g_race_line_t *G_Race_ClientLine(const g_client_t *cl) {
  return &g_race_lines[cl->ps.client];
}

static const char *G_Race_LinePath(pm_movement_t movement) {
  return va("records/%s-%s.ghost", g_level.name, Pm_Movement(movement)->name);
}

static const char *G_Race_LineTime(uint32_t ms) {
  return va("%u:%02u.%03u", ms / 60000, ms / 1000 % 60, ms % 1000);
}

/**
 * @brief Room for one more sample in `line`, in memory of `tag`.
 */
static g_race_sample_t *G_Race_AddSample(g_race_line_t *line, mem_tag_t tag) {

  if (line->count == RACE_MAX_SAMPLES) {
    return NULL;
  }

  if (line->count == line->capacity) {
    const size_t capacity = line->capacity ? line->capacity * 2 : 1024;
    g_race_sample_t *samples = gi.Malloc(capacity * sizeof(g_race_sample_t), tag);

    if (line->samples) {
      memcpy(samples, line->samples, line->count * sizeof(g_race_sample_t));
      gi.Free(line->samples);
    }

    line->samples = samples;
    line->capacity = capacity;
  }

  return &line->samples[line->count++];
}

static void G_Race_FreeLine(g_race_line_t *line) {

  gi.Free(line->samples);
  memset(line, 0, sizeof(*line));
}

// ---------------------------------------------------------------- the file

void G_Race_LoadLine(void) {

  gi.Free(g_level.race_line.samples); // the record was just beaten, or this is the same map again
  memset(&g_level.race_line, 0, sizeof(g_level.race_line));
  g_level.race_line_holder[0] = g_level.race_line_client[0] = '\0';
  g_level.race_line_time = 0;

  const char *path = G_Race_LinePath(g_level.movement);

  void *buffer;
  if (gi.LoadFile(path, &buffer) <= 0) {
    gi.SetConfigString(CS_RACE_GHOST, "");
    return;
  }

  const char *bsp = gi.GetConfigString(CS_BSP_HASH);
  size_t expected = 0;
  bool header = true, valid = true;

  char *line = buffer, *next;
  for (; line && *line && valid; line = next) {

    next = strchr(line, '\n');
    if (next) {
      *next++ = '\0';
    }

    if (*line == '\0' || (line[0] == '/' && line[1] == '/')) {
      continue;
    }

    if (header) {
      char key[32];
      const char *value = line;

      if (sscanf(line, "%31s", key) != 1) {
        continue;
      }

      value += q_strlen(key);
      while (*value == ' ') {
        value++;
      }

      if (!q_strcmp(key, "holder")) {
        q_strlcpy(g_level.race_line_holder, value, sizeof(g_level.race_line_holder));
      } else if (!q_strcmp(key, "client")) {
        q_strlcpy(g_level.race_line_client, value, sizeof(g_level.race_line_client));
      } else if (!q_strcmp(key, "time")) {
        g_level.race_line_time = (uint32_t) strtoul(value, NULL, 10);
      } else if (!q_strcmp(key, "bsp")) {
        if (q_strcmp(value, bsp)) {
          G_Warn("%s was set on another build of %s; ignoring it\n", path, g_level.name);
          valid = false;
        }
      } else if (!q_strcmp(key, "samples")) {
        expected = strtoul(value, NULL, 10);
        header = false;
      }
      continue;
    }

    g_race_sample_t sample;
    int32_t animation1, animation2;

    const uint32_t previous = g_level.race_line.count
                              ? g_level.race_line.samples[g_level.race_line.count - 1].time
                              : 0;

    if (sscanf(line, "%u %f %f %f %f %f %f %d %d", &sample.time,
               &sample.origin.x, &sample.origin.y, &sample.origin.z,
               &sample.angles.x, &sample.angles.y, &sample.angles.z,
               &animation1, &animation2) != 9 ||
        sample.time < previous || animation1 < 0 || animation1 > UINT8_MAX || animation2 < 0 || animation2 > UINT8_MAX) {
      G_Warn("%s has a sample it should not: \"%s\"\n", path, line);
      valid = false;
      break;
    }

    sample.animation1 = animation1;
    sample.animation2 = animation2;

    g_race_sample_t *added = G_Race_AddSample(&g_level.race_line, MEM_TAG_GAME_LEVEL);
    if (!added) {
      break;
    }

    *added = sample;
  }

  gi.FreeFile(buffer);

  if (valid && g_level.race_line.count != expected) {
    G_Warn("%s promised %zu samples and has %zu\n", path, expected, g_level.race_line.count);
    valid = false;
  }

  if (!valid || !g_level.race_line.count) {
    G_Race_FreeLine(&g_level.race_line);
    g_level.race_line_time = 0;
  }

  gi.SetConfigString(CS_RACE_GHOST, g_level.race_line.count ? g_level.race_line_client : "");
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

/**
 * @brief Writes `cl`'s line as the course record's, and makes it the level's.
 */
static void G_Race_SaveLine(g_client_t *cl) {
  const g_race_run_t *run = &cl->race_run;
  const g_race_line_t *line = G_Race_ClientLine(cl);
  const char *path = G_Race_LinePath(run->movement);

  file_t *file = gi.OpenFileWrite(path);
  if (!file) {
    G_Warn("Failed to open %s for writing\n", path);
    return;
  }

  const char *client = gi.GetConfigString(CS_CLIENTS + cl->ps.client);

  G_Race_WriteLine(file, "// Race line for %s under %s: the course record, %s in %s\n", g_level.name,
                   Pm_Movement(run->movement)->name, cl->persistent.net_name, G_Race_LineTime(run->elapsed));
  G_Race_WriteLine(file, "// samples are: time x y z pitch yaw roll animation1 animation2\n");
  G_Race_WriteLine(file, "holder %s\n", cl->persistent.net_name);
  G_Race_WriteLine(file, "guid %s\n", cl->persistent.guid);
  G_Race_WriteLine(file, "client %s\n", client);
  G_Race_WriteLine(file, "time %u\n", run->elapsed);
  G_Race_WriteLine(file, "bsp %s\n", gi.GetConfigString(CS_BSP_HASH));
  G_Race_WriteLine(file, "samples %zu\n", line->count);

  for (size_t i = 0; i < line->count; i++) {
    const g_race_sample_t *s = &line->samples[i];

    G_Race_WriteLine(file, "%u %.2f %.2f %.2f %.1f %.1f %.1f %u %u\n", s->time,
                     s->origin.x, s->origin.y, s->origin.z, s->angles.x, s->angles.y, s->angles.z,
                     s->animation1, s->animation2);
  }

  gi.CloseFile(file);

  if (run->movement == g_level.movement) {
    G_Race_LoadLine();
  }
}

// ---------------------------------------------------------------- the run's line

void G_Race_BeginLine(g_client_t *cl) {

  G_Race_DropLine(cl);
  G_Race_SampleLine(cl);
}

void G_Race_SampleLine(g_client_t *cl) {

  if (G_Race_Mode(cl) != RACE_MODE_RACE) { // a practice run is nobody's record
    return;
  }

  g_race_line_t *line = G_Race_ClientLine(cl);
  const uint32_t time = g_level.time - cl->race_run.start_time;

  // a client may move more than once a tick; the tick's sample is where it ended
  g_race_sample_t *sample = line->count && line->samples[line->count - 1].time == time
                            ? &line->samples[line->count - 1]
                            : G_Race_AddSample(line, MEM_TAG_GAME);
  if (!sample) {
    return;
  }

  const g_entity_t *ent = cl->entity;

  sample->time = time;
  sample->origin = ent->s.origin;
  sample->angles = ent->s.angles;
  sample->animation1 = ent->s.animation1;
  sample->animation2 = ent->s.animation2;
}

void G_Race_KeepLine(g_client_t *cl) {
  const g_race_line_t *line = G_Race_ClientLine(cl);

  if (line->count == RACE_MAX_SAMPLES) {
    G_Warn("%s's course record on %s outran the raceline; not kept\n", cl->persistent.net_name, g_level.name);
  } else if (line->count) {
    G_Race_SaveLine(cl);
  }

  G_Race_DropLine(cl);
}

void G_Race_DropLine(g_client_t *cl) {
  G_Race_FreeLine(G_Race_ClientLine(cl));
}

void G_Race_Shutdown(void) {

  for (size_t i = 0; i < lengthof(g_race_lines); i++) {
    G_Race_FreeLine(&g_race_lines[i]);
  }
}

// ---------------------------------------------------------------- the ghost

/**
 * @brief Moves the ghost to where the record was at this point in the run,
 * and lets it go once the record is over.
 */
static void G_Race_Ghost_Think(g_entity_t *ent) {
  const g_race_line_t *line = &g_level.race_line;

  if (!ent->owner || !ent->owner->in_use || !ent->owner->client || ent->owner->client->race_ghost != ent) {
    G_FreeEntity(ent);
    return;
  }

  const uint32_t time = g_level.time - ent->timestamp;

  while (ent->count + 1 < (int32_t) line->count && line->samples[ent->count + 1].time <= time) {
    ent->count++;
  }

  if ((size_t) ent->count + 1 >= line->count) {
    ent->owner->client->race_ghost = NULL;
    G_FreeEntity(ent);
    return;
  }

  const g_race_sample_t *sample = &line->samples[ent->count];

  ent->s.origin = sample->origin;
  ent->s.angles = sample->angles;
  ent->s.animation1 = sample->animation1;
  ent->s.animation2 = sample->animation2;

  gi.LinkEntity(ent);

  ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
}

void G_Race_SpawnGhost(g_client_t *cl) {

  G_Race_RemoveGhost(cl);

  if (!cl->persistent.race_ghost || !g_level.race_line.count) {
    return;
  }

  g_entity_t *ent = G_AllocEntity(__func__);

  ent->owner = cl->entity;
  ent->solid = SOLID_NOT;
  ent->move_type = MOVE_TYPE_NONE;
  ent->clip_mask = 0;

  ent->s.client = cl->entity->s.client;
  ent->s.model1 = MODEL_CLIENT;
  ent->s.effects = RACE_GHOST_EFFECTS;
  ent->s.origin = g_level.race_line.samples[0].origin;
  ent->s.angles = g_level.race_line.samples[0].angles;

  ent->timestamp = cl->race_run.start_time;
  ent->count = 0;
  ent->Think = G_Race_Ghost_Think;
  ent->next_think = g_level.time + QUETOO_TICK_MILLIS;

  gi.LinkEntity(ent);

  cl->race_ghost = ent;
}

void G_Race_RemoveGhost(g_client_t *cl) {

  if (cl->race_ghost) {
    G_FreeEntity(cl->race_ghost);
    cl->race_ghost = NULL;
  }
}

/**
 * @brief Toggles racing against the course record's ghost.
 */
void G_Race_Ghost_f(g_client_t *cl) {

  cl->persistent.race_ghost = !cl->persistent.race_ghost;

  if (!cl->persistent.race_ghost) {
    G_Race_RemoveGhost(cl);
    gi.ClientPrint(cl, PRINT_HIGH, "Ghost off\n");
    return;
  }

  if (g_level.race_line.count) {
    gi.ClientPrint(cl, PRINT_HIGH, "Ghost on: %s, %s, from your next start\n",
                   g_level.race_line_holder, G_Race_LineTime(g_level.race_line_time));
  } else {
    gi.ClientPrint(cl, PRINT_HIGH, "Ghost on, once someone sets a course record under %s\n",
                   Pm_Movement(g_level.movement)->name);
  }
}
