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

#include "ui_local.h"
#include "cl_local.h"

#include "ui_console.h"

#include "ui_diagnostics.h"

#define NET_GRAPH_WIDTH 128
#define NET_GRAPH_HEIGHT 48
#define STATS_TOP 64

typedef struct {
  float value;
  color_t color;
} net_graph_sample_t;

static net_graph_sample_t net_graph_samples[NET_GRAPH_WIDTH];
static int32_t num_net_graph_samples;

void Ui_AddNetGraphSample(float value, const color_t color) {

  net_graph_samples[num_net_graph_samples].value = Minf(value, 1.f);
  net_graph_samples[num_net_graph_samples].color = color;

  num_net_graph_samples = (num_net_graph_samples + 1) % NET_GRAPH_WIDTH;
}

#pragma mark - NetGraphView

typedef struct {
  View view;
  ViewInterface *interface[0];
} NetGraphView;

typedef struct {
  ViewInterface viewInterface;
} NetGraphViewInterface;

static Class *_NetGraphView(void);

#define _Class _NetGraphView

/**
 * @see View::render(View *, Renderer *)
 */
static void render(View *self, Renderer *renderer) {

  super(View, self, render, renderer);

  const SDL_Rect frame = $(self, renderFrame);

  for (int32_t i = 0; i < NET_GRAPH_WIDTH; i++) {

    const int32_t j = (num_net_graph_samples - 1 - i) & (NET_GRAPH_WIDTH - 1);
    const int32_t h = net_graph_samples[j].value * frame.h;

    if (!h) {
      continue;
    }

    const int32_t x = frame.x + frame.w - 1 - i;
    const int32_t y = frame.y + frame.h - 1;

    const SDL_Point points[] = { { x, y }, { x, y - h } };
    const color32_t c = Color_Color32(net_graph_samples[j].color);

    $(renderer, drawLines, points, lengthof(points), &(const SDL_Color) { c.r, c.g, c.b, c.a });
  }
}

static void initializeNetGraphView(Class *clazz) {
  ((ViewInterface *) clazz->interface)->render = render;
}

static Class *_NetGraphView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "NetGraphView",
      .superclass = _View(),
      .instanceSize = sizeof(NetGraphView),
      .interfaceSize = sizeof(NetGraphViewInterface),
      .initialize = initializeNetGraphView,
    });
  });

  return clazz;
}

#undef _Class

#pragma mark - DiagnosticsViewController

#define _Class _DiagnosticsViewController

/**
 * @brief A monospace Text in the given color, aligned as given.
 */
static Text *addText(View *view, ViewAlignment alignment, const SDL_Color *color) {

  Text *text = $(alloc(Text), initWithText, NULL, NULL);
  assert(text);

  text->view.alignment = alignment;

  $(text->view.style, addCharactersAttribute, "font-family", DEFAULT_MONOSPACE_FONT_FAMILY);
  $(text->view.style, addIntegerAttribute, "font-size", 14);
  $(text->view.style, addColorAttribute, "color", color);

  $(view, addSubview, (View *) text);

  return text;
}

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  DiagnosticsViewController *this = (DiagnosticsViewController *) self;

  release(this->netGraph);
  release(this->counters);
  release(this->stats);
  release(this->rendererStats);
  release(this->soundStats);

  super(Object, self, dealloc);
}

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  self->view->pointerEvents = false;

  DiagnosticsViewController *this = (DiagnosticsViewController *) self;

  this->netGraph = $((View *) alloc(NetGraphView), initWithFrame, &MakeRect(0, 0, NET_GRAPH_WIDTH, NET_GRAPH_HEIGHT));
  assert(this->netGraph);

  this->netGraph->alignment = ViewAlignmentBottomRight;
  this->netGraph->pointerEvents = false;
  $(this->netGraph->style, addColorAttribute, "background-color", &(const SDL_Color) { 64, 64, 64, 128 });

  $(self->view, addSubview, this->netGraph);

  this->counters = addText(self->view, ViewAlignmentBottomRight, &Colors.White);
  this->counters->view.pointerEvents = false;

  this->stats = (View *) $(alloc(StackView), initWithFrame, NULL);
  assert(this->stats);

  this->stats->autoresizingMask = ViewAutoresizingContain;
  this->stats->pointerEvents = false;
  $(this->stats->style, addRectangleAttribute, "padding", &MakeRect(STATS_TOP, 0, 0, 1));
  $(this->stats->style, addIntegerAttribute, "spacing", 14);

  this->rendererStats = addText(this->stats, ViewAlignmentNone, &(const SDL_Color) { 255, 255, 0, 255 });
  this->rendererStats->view.pointerEvents = false;

  this->soundStats = addText(this->stats, ViewAlignmentNone, &(const SDL_Color) { 255, 0, 255, 255 });
  this->soundStats->view.pointerEvents = false;

  $(self->view, addSubview, this->stats);
}

/**
 * @brief Formats the current value and the min and max over the sample window.
 */
static void formatCounter(char *buffer, size_t size, const char *title, const uint16_t *samples) {

  uint16_t min, max;

  if (!cl.sample_count) {
    min = max = samples[cl.sample_index];
  } else {
    min = UINT16_MAX;
    max = 0;

    for (uint32_t i = 0; i < cl.sample_count; i++) {
      int32_t index = cl.sample_index - i;

      if (index < 0) {
        index = STAT_COUNTER_SAMPLE_COUNT - (-index);
      }

      const uint16_t sample = samples[index];
      min = min(min, sample);
      max = max(max, sample);
    }
  }

  q_snprintf(buffer, size, "%3u%s (^1%3u ^2%3u^7)", samples[cl.sample_index], title, min, max);
}

/**
 * @brief The speed, frame and packet counters, and the position when asked for.
 */
static void updateCounters(DiagnosticsViewController *self) {

  static char fps[28], pps[28], spd[8], pos[32];
  static int32_t last_draw_time, last_speed_time;

  cl.frame_counter[cl.sample_index]++;

  if (quetoo.ticks - last_speed_time >= 100) {
    vec3_t velocity = cl.frame.ps.pm_state.velocity;
    velocity.z = 0.f;

    q_snprintf(spd, sizeof(spd), "%4.0fspd", Vec3_Length(velocity));

    last_speed_time = quetoo.ticks;
  }

  if (quetoo.ticks - last_draw_time >= 1000) {
    formatCounter(fps, sizeof(fps), "fps", cl.frame_counter);
    formatCounter(pps, sizeof(pps), "pps", cl.packet_counter);

    last_draw_time = quetoo.ticks;

    cl.sample_index = (cl.sample_index + 1) % STAT_COUNTER_SAMPLE_COUNT;
    cl.sample_count = min(STAT_COUNTER_SAMPLE_COUNT, cl.sample_count + 1);

    cl.frame_counter[cl.sample_index] = 0;
    cl.packet_counter[cl.sample_index] = 0;
  }

  if (cl_draw_position->integer) {
    q_snprintf(pos, sizeof(pos), "%4.0f %4.0f %4.0f\n",
               cl.frame.ps.pm_state.origin.x, cl.frame.ps.pm_state.origin.y, cl.frame.ps.pm_state.origin.z);
  } else {
    pos[0] = '\0';
  }

  $(self->counters, setTextWithFormat, "%s%16s\n%s\n%s", pos, spd, fps, pps);
}

/**
 * @brief Appends to a bounded buffer, holding `len` at the buffer's end once it is full.
 */
static void append(char *text, size_t size, size_t *len, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
static void append(char *text, size_t size, size_t *len, const char *fmt, ...) {

  if (*len >= size - 1) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  const int n = vsnprintf(text + *len, size - *len, fmt, args);
  va_end(args);

  *len = n < 0 ? *len : min(*len + (size_t) n, size - 1);
}

/**
 * @brief The renderer stats, as Cl_DrawRendererStats laid them out, less the Draw 2D counts
 * that go away with it.
 */
static void updateRendererStats(DiagnosticsViewController *self) {

  static char sprites[64], beams[64], instances[64], draw_elements[64], decals[64], decal_draw_elements[64];
  static uint32_t sprite_time;

  if (quetoo.ticks - sprite_time > 100) {
    sprite_time = quetoo.ticks;
    q_snprintf(sprites, sizeof(sprites), " %d sprites", cl_view.num_sprites);
    q_snprintf(beams, sizeof(beams), " %d beams", cl_view.num_beams);
    q_snprintf(instances, sizeof(instances), " %d instances", cl_view.num_sprite_instances);
    q_snprintf(draw_elements, sizeof(draw_elements), " %d draw elements", r_stats.sprite_draw_elements);
    q_snprintf(decals, sizeof(decals), " %d decals", r_stats.decals);
    q_snprintf(decal_draw_elements, sizeof(decal_draw_elements), " %d draw elements", r_stats.decal_draw_elements);
  }

  char text[2048] = "";
  size_t len = 0;

  append(text, sizeof(text), &len,
    "Occlusion queries:\n %d queries allocated\n %d queries visible\n %d queries occluded\n\n",
    r_stats.queries_allocated, r_stats.queries_visible, r_stats.queries_occluded);

  append(text, sizeof(text), &len,
    "Lights:\n %d lights allocated\n %d lights visible\n %d lights occluded\n %d lights cached\n\n",
    r_stats.lights_visible + r_stats.lights_occluded, r_stats.lights_visible, r_stats.lights_occluded, r_stats.lights_cached);

  append(text, sizeof(text), &len,
    "BSP:\n %d entities\n %d draw elements\n %d triangles\n\n",
    r_stats.bsp_inline_models, r_stats.bsp_draw_elements, r_stats.bsp_triangles);

  append(text, sizeof(text), &len,
    "Mesh:\n %d entities\n %d draw elements\n %d triangles\n\n",
    r_stats.mesh_models, r_stats.mesh_draw_elements, r_stats.mesh_triangles);

  append(text, sizeof(text), &len,
    "Sprites:\n%s\n%s\n%s\n%s\n%s\n%s\n\n",
    sprites, beams, instances, draw_elements, decals, decal_draw_elements);

  append(text, sizeof(text), &len,
    "Leaf: %d", Cm_PointLeafnum(cl_view.origin, 0));

  const vec3_t forward = Vec3_Fmaf(cl_view.origin, MAX_WORLD_DIST, cl_view.forward);
  const cm_trace_t tr = Cl_Trace(cl_view.origin, forward, Box3_Zero(), 0, CONTENTS_MASK_VISIBLE);

  if (tr.material) {
    append(text, sizeof(text), &len, "\n\n^7%s (%g %g %g) %g",
               tr.material->name, tr.plane.normal.x, tr.plane.normal.y, tr.plane.normal.z, tr.plane.dist);
  }

  $(self->rendererStats, setText, text);
}

/**
 * @brief The sound stats, as Cl_DrawSoundStats laid them out.
 */
static void updateSoundStats(DiagnosticsViewController *self) {

  char text[16384] = "";
  size_t len = 0;

  append(text, sizeof(text), &len, "Sound:\n%d channels  reverb %.2f", s_context.num_active_channels, s_context.reverb);

  for (int32_t i = 0; i < MAX_CHANNELS; i++) {
    const s_channel_t *channel = &s_context.channels[i];

    if (!channel->play.sample) {
      continue;
    }

    ALenum state;
    alGetSourcei(s_context.sources[i], AL_SOURCE_STATE, &state);

    if (state != AL_PLAYING) {
      continue;
    }

    append(text, sizeof(text), &len, "\n  %i: %s @ (%f %f %f) : %i : (%.2f occluded)",
           i, channel->play.sample->media.name, channel->play.origin.x, channel->play.origin.y,
           channel->play.origin.z, channel->play.flags, channel->occlusion);
  }

  $(self->soundStats, setText, text);
}

/**
 * @fn void DiagnosticsViewController::update(DiagnosticsViewController *self)
 * @memberof DiagnosticsViewController
 */
static void update(DiagnosticsViewController *self) {

  const bool active = cls.state == CL_ACTIVE;
  const bool game = active && cls.key_state.dest == KEY_GAME;
  const bool console = active && cls.key_state.dest == KEY_CONSOLE;

  const int32_t height = self->viewController.view->frame.h;
  const int32_t top = console && height > 0 ? Ui_ConsoleHeight(height) : STATS_TOP;
  if (self->stats->padding.top != top) {
    self->stats->padding.top = top;
    $(self->stats->style, addRectangleAttribute, "padding", &MakeRect(top, 0, 0, 1));
    $(self->stats, setNeedsLayout);
  }

  $(self->netGraph, setHidden, !(active && cl_draw_net_graph->value));

  const bool counters = active && cl_draw_counters->integer;
  $((View *) self->counters, setHidden, !counters);
  if (counters) {
    updateCounters(self);
  }

  const bool renderer = (game || console) && r_draw_stats->value;
  $((View *) self->rendererStats, setHidden, !renderer);
  if (renderer) {
    updateRendererStats(self);
  }

  const bool sound = game && s_draw_stats->value;
  $((View *) self->soundStats, setHidden, !sound);
  if (sound) {
    updateSoundStats(self);
  }

  $(self->stats, setHidden, !(renderer || sound));
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;

  ((DiagnosticsViewControllerInterface *) clazz->interface)->update = update;
}

/**
 * @fn Class *DiagnosticsViewController::_DiagnosticsViewController(void)
 * @memberof DiagnosticsViewController
 */
Class *_DiagnosticsViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "DiagnosticsViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(DiagnosticsViewController),
      .interfaceSize = sizeof(DiagnosticsViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
