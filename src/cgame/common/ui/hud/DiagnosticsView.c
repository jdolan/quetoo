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

#include "cg_local.h"

#include "DiagnosticsView.h"

#define _Class _DiagnosticsView

#define DIAGNOSTICS_REFRESH_INTERVAL 250

static const char *_name = "name";
static const char *_value = "value";

#pragma mark - Rows

/**
 * @brief Appends a row, discarding it when the table is full.
 */
static void addRow(DiagnosticsView *self, const char *name, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void addRow(DiagnosticsView *self, const char *name, const char *fmt, ...) {

  if (self->num_rows == DIAGNOSTICS_MAX_ROWS) {
    return;
  }

  q_strlcpy(self->rows[self->num_rows].name, name, DIAGNOSTICS_ROW_NAME);

  va_list args;
  va_start(args, fmt);
  vsnprintf(self->rows[self->num_rows].value, DIAGNOSTICS_ROW_VALUE, fmt, args);
  va_end(args);

  self->num_rows++;
}

/**
 * @brief Appends the current value and the min and max over the sample window.
 */
static void addCounterRow(DiagnosticsView *self, const char *name, const uint16_t *samples) {

  const cl_client_t *cl = cgi.client;

  uint16_t min = samples[cl->sample_index], max = min;

  for (uint32_t i = 0; i < cl->sample_count; i++) {
    int32_t index = cl->sample_index - i;

    if (index < 0) {
      index += STAT_COUNTER_SAMPLE_COUNT;
    }

    min = Mini(min, samples[index]);
    max = Maxi(max, samples[index]);
  }

  addRow(self, name, "%u (%u - %u)", samples[cl->sample_index], min, max);
}

/**
 * @brief Rebuilds the rows from the client, the view and the stage.
 */
static void refresh(DiagnosticsView *self, const cl_frame_t *frame) {

  const cl_client_t *cl = cgi.client;
  const r_view_t *view = cgi.view;
  const r_view_stats_t *r = &view->stats;
  const s_stage_stats_t *s = &cgi.stage->stats;

  self->num_rows = 0;

  const vec3_t origin = frame->ps.pm_state.origin;
  addRow(self, "origin", "%.0f %.0f %.0f", origin.x, origin.y, origin.z);

  vec3_t velocity = frame->ps.pm_state.velocity;
  velocity.z = 0.f;
  addRow(self, "speed", "%.0f", Vec3_Length(velocity));

  addRow(self, "leaf", "%d", cgi.PointLeafnum(view->origin, 0));

  const vec3_t end = Vec3_Fmaf(view->origin, MAX_WORLD_DIST, view->forward);
  const cm_trace_t tr = cgi.Trace(view->origin, end, Box3_Zero(), NULL, CONTENTS_MASK_VISIBLE);
  if (tr.material) {
    addRow(self, "surface", "%s (%g %g %g) %g", tr.material->name,
           tr.plane.normal.x, tr.plane.normal.y, tr.plane.normal.z, tr.plane.dist);
  }

  addRow(self, "fps", "%d", self->fps);
  addCounterRow(self, "pps", cl->packet_counter);
  addRow(self, "ping", "%u ms", cl->ping);
  addRow(self, "dropped", "%u", cl->dropped);

  addRow(self, "queries", "%d allocated, %d visible, %d occluded",
         r->queries_allocated, r->queries_visible, r->queries_occluded);
  addRow(self, "lights", "%d visible, %d occluded, %d cached",
         r->lights_visible, r->lights_occluded, r->lights_cached);
  addRow(self, "entities", "%d visible, %d occluded", r->entities_visible, r->entities_occluded);
  addRow(self, "blocks", "%d visible, %d occluded", r->blocks_visible, r->blocks_occluded);
  addRow(self, "bsp", "%d models, %d draws, %d triangles",
         r->bsp_inline_models, r->bsp_draw_elements, r->bsp_triangles);
  addRow(self, "mesh", "%d models, %d draws, %d triangles",
         r->mesh_models, r->mesh_draw_elements, r->mesh_triangles);
  addRow(self, "sprites", "%d sprites, %d beams, %d instances, %d draws",
         view->num_sprites, view->num_beams, view->num_sprite_instances, r->sprite_draw_elements);
  addRow(self, "decals", "%d draws", r->decal_draw_elements);

  addRow(self, "sound", "%d channels, reverb %.2f", s->num_channels, s->reverb);

  for (int32_t i = 0; i < s->num_channels; i++) {
    const s_stage_channel_t *c = &s->channels[i];

    addRow(self, va("channel %d", i), "%s (%.0f %.0f %.0f) %d %.2f",
           c->name, c->origin.x, c->origin.y, c->origin.z, c->flags, c->occlusion);
  }
}

#pragma mark - TableViewDataSource

/**
 * @see TableViewDataSource::numberOfRows
 */
static size_t numberOfRows(const TableView *tableView) {
  return ((DiagnosticsView *) tableView)->num_rows;
}

#pragma mark - TableViewDelegate

/**
 * @see TableViewDelegate::cellForColumnAndRow
 */
static TableCellView *cellForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

  const DiagnosticsView *this = (DiagnosticsView *) tableView;

  TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);
  assert(cell);

  if (q_strcmp(column->identifier, _name) == 0) {
    $(cell->text, setText, this->rows[row].name);
    $((View *) cell->text, addClassName, "caption");
  } else {
    $(cell->text, setText, this->rows[row].value);
    $((View *) cell->text, addClassName, "live");
  }

  return cell;
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = (View *) super(TableView, self, initWithFrame, NULL);
  if (self) {
    DiagnosticsView *this = (DiagnosticsView *) self;
    TableView *table = (TableView *) self;

    self->autoresizingMask = ViewAutoresizingContain;

    $(table, addColumnWithIdentifier, _name);
    $(table, addColumnWithIdentifier, _value);

    table->dataSource.numberOfRows = numberOfRows;
    table->dataSource.self = this;

    table->delegate.cellForColumnAndRow = cellForColumnAndRow;
    table->delegate.self = this;

    $((View *) table->headerView, setHidden, true);
  }

  return self;
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  DiagnosticsView *this = (DiagnosticsView *) self;

  $(self, setHidden, !cg_draw_diagnostics->integer);

  if (data && !self->hidden) {
    cl_client_t *cl = cgi.client;

    this->frames++;

    const uint32_t now = (uint32_t) SDL_GetTicks();
    if (now - this->time >= 1000) {
      this->fps = this->frames;
      this->frames = 0;
      this->time = now;

      cl->sample_index = (cl->sample_index + 1) % STAT_COUNTER_SAMPLE_COUNT;
      cl->sample_count = Mini(STAT_COUNTER_SAMPLE_COUNT, cl->sample_count + 1);
      cl->packet_counter[cl->sample_index] = 0;
    }

    if (now - this->refresh_time >= DIAGNOSTICS_REFRESH_INTERVAL) {
      this->refresh_time = now;

      refresh(this, (const cl_frame_t *) data);
      $((TableView *) self, reloadData);
    }
  }

  super(View, self, updateBindings, data);
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

/**
 * @fn Class *DiagnosticsView::_DiagnosticsView(void)
 * @memberof DiagnosticsView
 */
Class *_DiagnosticsView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "DiagnosticsView",
      .superclass = _TableView(),
      .instanceSize = sizeof(DiagnosticsView),
      .interfaceSize = sizeof(DiagnosticsViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
