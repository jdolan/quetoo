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

#include "cg_race.h"

#include "ui/hud/CounterView.h"
#include "ui/hud/OverlayText.h"

/**
 * @file
 * @brief The HUD, arranged for racing. Health, ammo, armor and the powerups stay
 * as common draws them - a rocket jump wants both - and the frags and deaths
 * go, since nobody is scoring any; in their column are the speed and the runs
 * so far. The run's time sits across the top with the checkpoints beneath it.
 *
 * Everything here is read from the player state the server already sends, or
 * measured on the client; the speed in particular is the client's own, not
 * the server's word for it.
 */

// how much of each frame's speed the readout takes on; shown raw, it flickers
// with every frame at a high frame rate
#define RACE_HUD_SPEED_LERP .2f

/**
 * @see cg_race.h
 */
const char *Cg_Race_FormatTime(uint32_t ms) {
  return va("%u:%02u.%03u", ms / 60000, ms / 1000 % 60, ms % 1000);
}

// how long a milestone stays on the HUD
#define RACE_HUD_MILESTONE_MILLIS 3000

static struct {
  char name[MAX_QPATH];
  uint32_t time;
  int32_t vs_best, vs_record;
  uint32_t shown; // when it went up, in unclamped client time; 0 for none
} cg_race_milestone;

/**
 * @see cg_race.h
 */
void Cg_Race_Milestone(g_race_milestone_t kind, uint16_t number, const char *label, uint32_t time, int32_t vs_best, int32_t vs_record) {

  if (label && *label) {
    q_strlcpy(cg_race_milestone.name, label, sizeof(cg_race_milestone.name));
  } else {
    const char *kinds[] = { "Checkpoint", "Split", "Stage" };
    q_snprintf(cg_race_milestone.name, sizeof(cg_race_milestone.name), "%s %u", kinds[kind % 3], number);
  }

  cg_race_milestone.time = time;
  cg_race_milestone.vs_best = vs_best;
  cg_race_milestone.vs_record = vs_record;
  cg_race_milestone.shown = cgi.client->unclamped_time;
}

#pragma mark - RaceRunView

#define _Class _RaceRunView

/**
 * @brief The run in progress: its time, checkpoints, and the latest milestone against the
 * best and the record, for a moment.
 * @extends OverlayText
 */
typedef struct RaceRunViewInterface RaceRunViewInterface;

typedef struct {
  OverlayText overlayText;
  RaceRunViewInterface *interface[0];
} RaceRunView;

struct RaceRunViewInterface {
  OverlayTextInterface overlayTextInterface;
};

/**
 * @brief A signed delta against `against`, coloured by which way it went.
 */
static const char *Cg_Race_FormatDelta(int32_t delta, const char *against) {
  return va("%s%s%s  %s", delta > 0 ? "^1" : "^2", delta < 0 ? "-" : "+", Cg_Race_FormatTime(abs(delta)), against);
}

/**
 * @see OverlayText::textForFrame(OverlayText *, const cl_frame_t *)
 */
static const char *textForFrame(OverlayText *self, const cl_frame_t *frame) {

  const player_state_t *ps = &frame->ps;

  if (ps->stats[STAT_RACE_MODE] == RACE_MODE_SPECTATOR) {
    return NULL;
  }

  const g_race_run_state_t state = ps->stats[STAT_RACE_RUN];
  if (state == RACE_RUN_IDLE) {
    cg_race_milestone.shown = 0;
    return NULL;
  }

  const char *color = "^7";
  if (ps->stats[STAT_RACE_FLAGS]) {
    color = "^1";
  } else if (ps->stats[STAT_RACE_MODE] == RACE_MODE_PRACTICE) {
    color = "^3";
  } else if (state == RACE_RUN_FINISHED) {
    color = "^2";
  }

  static char text[MAX_STRING_CHARS];
  q_snprintf(text, sizeof(text), "%s%s", color, Cg_Race_FormatTime(Cg_Race_Time(ps)));

  uint32_t checkpoints = 0;
  sscanf(cgi.ConfigString(CS_RACE_COURSE), "%u", &checkpoints);

  if (checkpoints) {
    q_strlcat(text, va("\n^7%d / %u", ps->stats[STAT_RACE_CHECKPOINTS], checkpoints), sizeof(text));
  }

  if (cg_race_milestone.shown && cgi.client->unclamped_time - cg_race_milestone.shown < RACE_HUD_MILESTONE_MILLIS) {

    q_strlcat(text, va("\n^7%s  %s", cg_race_milestone.name, Cg_Race_FormatTime(cg_race_milestone.time)), sizeof(text));

    if (cg_race_milestone.vs_best != RACE_MILESTONE_NO_DELTA &&
        cg_race_milestone.vs_best != cg_race_milestone.vs_record) {
      q_strlcat(text, va("\n%s", Cg_Race_FormatDelta(cg_race_milestone.vs_best, "best")), sizeof(text));
    }

    if (cg_race_milestone.vs_record != RACE_MILESTONE_NO_DELTA) {
      q_strlcat(text, va("\n%s", Cg_Race_FormatDelta(cg_race_milestone.vs_record, "record")), sizeof(text));
    }
  }

  return text;
}

/**
 * @see Class::initialize(Class *)
 */
static void initializeRaceRunView(Class *clazz) {
  ((OverlayTextInterface *) clazz->interface)->textForFrame = textForFrame;
}

Class *_RaceRunView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "RaceRunView",
      .superclass = _OverlayText(),
      .instanceSize = sizeof(RaceRunView),
      .interfaceSize = sizeof(RaceRunViewInterface),
      .initialize = initializeRaceRunView,
    });
  });

  return clazz;
}

#undef _Class

#pragma mark - SpeedView

#define _Class _SpeedView

/**
 * @brief The speed counter: horizontal velocity, eased so it does not flicker per frame.
 * @extends CounterView
 */
typedef struct SpeedViewInterface SpeedViewInterface;

typedef struct {
  CounterView counterView;
  SpeedViewInterface *interface[0];
  float speed;
} SpeedView;

struct SpeedViewInterface {
  CounterViewInterface counterViewInterface;
};

/**
 * @see CounterView::valueForFrame(CounterView *, const cl_frame_t *)
 */
static int32_t valueForFrame(CounterView *self, const cl_frame_t *frame) {

  SpeedView *this = (SpeedView *) self;

  vec3_t velocity = frame->ps.pm_state.velocity;
  velocity.z = 0.f;

  this->speed += (Vec3_Length(velocity) - this->speed) * RACE_HUD_SPEED_LERP;

  return (int32_t) this->speed;
}

/**
 * @see Class::initialize(Class *)
 */
static void initializeSpeedView(Class *clazz) {
  ((CounterViewInterface *) clazz->interface)->valueForFrame = valueForFrame;
}

Class *_SpeedView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "SpeedView",
      .superclass = _CounterView(),
      .instanceSize = sizeof(SpeedView),
      .interfaceSize = sizeof(SpeedViewInterface),
      .initialize = initializeSpeedView,
    });
  });

  return clazz;
}

#undef _Class

#pragma mark - RunsView

#define _Class _RunsView

/**
 * @brief The runs started on this map.
 * @extends CounterView
 */
typedef struct RunsViewInterface RunsViewInterface;

typedef struct {
  CounterView counterView;
  RunsViewInterface *interface[0];
} RunsView;

struct RunsViewInterface {
  CounterViewInterface counterViewInterface;
};

/**
 * @see CounterView::valueForFrame(CounterView *, const cl_frame_t *)
 */
static int32_t runsForFrame(CounterView *self, const cl_frame_t *frame) {
  return frame->ps.stats[STAT_RACE_RUNS];
}

/**
 * @see Class::initialize(Class *)
 */
static void initializeRunsView(Class *clazz) {
  ((CounterViewInterface *) clazz->interface)->valueForFrame = runsForFrame;
}

Class *_RunsView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "RunsView",
      .superclass = _CounterView(),
      .instanceSize = sizeof(RunsView),
      .interfaceSize = sizeof(RunsViewInterface),
      .initialize = initializeRunsView,
    });
  });

  return clazz;
}

#undef _Class
