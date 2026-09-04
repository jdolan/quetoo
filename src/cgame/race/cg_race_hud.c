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

// where the run sits, from the top of the view
#define RACE_HUD_TOP 32

// how much of each frame's speed the readout takes on; shown raw, it flickers
// with every frame at a high frame rate
#define RACE_HUD_SPEED_LERP .2f

/**
 * @see cg_race.h
 */
const char *Cg_Race_FormatTime(uint32_t ms) {
  return va("%u:%02u.%03u", ms / 60000, ms / 1000 % 60, ms % 1000);
}

/**
 * @brief Draws a line centered on the view, as the run and its milestones are shown.
 */
static void Cg_Race_DrawCentered(int32_t y, const char *string, const color_t color) {
  cgi.Draw2DString((cgi.context->w - cgi.StringWidth(string)) / 2, y, string, color);
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

/**
 * @brief A comparison, signed, colored by which way it went.
 */
static void Cg_Race_DrawDelta(int32_t y, int32_t delta, const char *against) {
  const char *string = va("%s%s  %s", delta < 0 ? "-" : "+", Cg_Race_FormatTime(abs(delta)), against);

  Cg_Race_DrawCentered(y, string, delta > 0 ? color_red : color_green);
}

/**
 * @brief The time, colored by what will become of it, and the checkpoints
 * reached out of the course's.
 */
static void Cg_Race_DrawRun(const player_state_t *ps) {

  const g_race_run_state_t state = ps->stats[STAT_RACE_RUN];
  if (state == RACE_RUN_IDLE) {
    cg_race_milestone.shown = 0;
    return;
  }

  color_t color = color_white;
  if (ps->stats[STAT_RACE_FLAGS]) {
    color = color_red;
  } else if (ps->stats[STAT_RACE_MODE] == RACE_MODE_PRACTICE) {
    color = color_yellow;
  } else if (state == RACE_RUN_FINISHED) {
    color = color_green;
  }

  int32_t ch, y = RACE_HUD_TOP;

  cgi.BindFont("large", NULL, &ch);
  Cg_Race_DrawCentered(y, Cg_Race_FormatTime(Cg_Race_Time(ps)), color);
  y += ch;

  uint32_t checkpoints = 0;
  sscanf(cgi.ConfigString(CS_RACE_COURSE), "%u", &checkpoints);

  if (checkpoints) {
    cgi.BindFont("small", NULL, &ch);
    Cg_Race_DrawCentered(y, va("%d / %u", ps->stats[STAT_RACE_CHECKPOINTS], checkpoints), color_white);
    y += ch;
  }

  // the latest milestone, and how it compares, for a moment
  if (cg_race_milestone.shown && cgi.client->unclamped_time - cg_race_milestone.shown < RACE_HUD_MILESTONE_MILLIS) {
    cgi.BindFont("small", NULL, &ch);

    Cg_Race_DrawCentered(y, va("%s  %s", cg_race_milestone.name, Cg_Race_FormatTime(cg_race_milestone.time)), color_white);
    y += ch;

    if (cg_race_milestone.vs_best != RACE_MILESTONE_NO_DELTA &&
        cg_race_milestone.vs_best != cg_race_milestone.vs_record) {
      Cg_Race_DrawDelta(y, cg_race_milestone.vs_best, "best");
      y += ch;
    }

    if (cg_race_milestone.vs_record != RACE_MILESTONE_NO_DELTA) {
      Cg_Race_DrawDelta(y, cg_race_milestone.vs_record, "record");
    }
  }

  cgi.BindFont(NULL, NULL, NULL);
}

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

static int32_t valueForFrame(CounterView *self, const cl_frame_t *frame) {

  SpeedView *this = (SpeedView *) self;

  vec3_t velocity = frame->ps.pm_state.velocity;
  velocity.z = 0.f;

  this->speed += (Vec3_Length(velocity) - this->speed) * RACE_HUD_SPEED_LERP;

  return (int32_t) this->speed;
}

static void initialize(Class *clazz) {
  ((CounterViewInterface *) clazz->interface)->valueForFrame = valueForFrame;
}

static Class *_SpeedView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "SpeedView",
      .superclass = _CounterView(),
      .instanceSize = sizeof(SpeedView),
      .interfaceSize = sizeof(SpeedViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class

void Cg_Race_ConfigureHud(View *hud) {

  const char *removed[] = { "frags", "deaths" };
  for (size_t i = 0; i < lengthof(removed); i++) {
    View *view = $(hud, descendantWithIdentifier, removed[i]);
    if (view) {
      $(view, removeFromSuperview);
    }
  }

  View *stats = $(hud, descendantWithIdentifier, "stats");
  if (stats == NULL) {
    return;
  }

  View *time = $(stats, subviewWithIdentifier, "time");

  CounterView *speed = $((CounterView *) alloc(SpeedView), initWithCaption, "Speed", COUNTER_VIEW_NO_STAT);
  CounterView *runs = $(alloc(CounterView), initWithCaption, "Runs", STAT_RACE_RUNS);

  $(stats, addSubview, (View *) speed);
  $(stats, addSubview, (View *) runs);

  // keep the clock beneath the counters
  if (time) {
    $(stats, bringSubviewToFront, time);
  }

  release(speed);
  release(runs);
}

void Cg_Race_DrawHud(const player_state_t *ps) {

  Cg_DrawSpectator(ps);

  Cg_DrawChase(ps);

  if (ps->stats[STAT_RACE_MODE] == RACE_MODE_SPECTATOR) {
    return;
  }

  Cg_Race_DrawRun(ps);
}
