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

  if (self->numRows == DIAGNOSTICS_MAX_ROWS) {
    return;
  }

  q_strlcpy(self->rows[self->numRows].name, name, DIAGNOSTICS_ROW_NAME);

  va_list args;
  va_start(args, fmt);
  vsnprintf(self->rows[self->numRows].value, DIAGNOSTICS_ROW_VALUE, fmt, args);
  va_end(args);

  self->numRows++;
}

/**
 * @brief Rebuilds the rows from the client, the view and the stage.
 */
static void refresh(DiagnosticsView *self, const ClientFrame *frame) {

  const Client *cl = cgi.client;
  const RenderView *view = cgi.view;
  const RenderViewStats *r = &view->stats;
  const SoundStageStats *s = &cgi.stage->stats;

  self->numRows = 0;

  const Vec3 origin = frame->ps.pmState.origin;
  addRow(self, "origin", "%.0f %.0f %.0f", origin.x, origin.y, origin.z);

  Vec3 velocity = frame->ps.pmState.velocity;
  velocity.z = 0.f;
  addRow(self, "speed", "%.0f", Vec3_Length(velocity));

  addRow(self, "leaf", "%d", cgi.PointLeafnum(view->origin, 0));

  const Vec3 end = Vec3_Fmaf(view->origin, MAX_WORLD_DIST, view->forward);
  const CmTrace tr = cgi.Trace(view->origin, end, Box3_Zero(), NULL, CONTENTS_MASK_VISIBLE);
  if (tr.material) {
    addRow(self, "surface", "%s (%g %g %g) %g", tr.material->name,
           tr.plane.normal.x, tr.plane.normal.y, tr.plane.normal.z, tr.plane.dist);
  }

  addRow(self, "fps", "%d", self->fps);
  addRow(self, "pps", "%d", self->pps);
  addRow(self, "ping", "%d ms", frame->ps.stats[STAT_PING]);
  addRow(self, "dropped", "%u", cl->dropped);

  addRow(self, "queries", "%d allocated, %d visible, %d occluded",
         r->queriesAllocated, r->queriesVisible, r->queriesOccluded);
  addRow(self, "lights", "%d visible, %d occluded, %d cached",
         r->lightsVisible, r->lightsOccluded, r->lightsCached);
  addRow(self, "entities", "%d visible, %d occluded", r->entitiesVisible, r->entitiesOccluded);
  addRow(self, "blocks", "%d visible, %d occluded", r->blocksVisible, r->blocksOccluded);
  addRow(self, "portals", "%d offered, %d drawn, %d triangles", r->portalsOffered, r->portalsDrawn, r->portalsTriangles);
  addRow(self, "bsp", "%d models, %d draws, %d triangles",
         r->bspInlineModels, r->bspDrawElements, r->bspTriangles);
  addRow(self, "mesh", "%d models, %d draws, %d triangles",
         r->meshModels, r->meshDrawElements, r->meshTriangles);
  addRow(self, "sprites", "%d sprites, %d beams, %d instances, %d draws",
         view->numSprites, view->numBeams, view->numSpriteInstances, r->spriteDrawElements);
  addRow(self, "decals", "%d draws", r->decalDrawElements);

  addRow(self, "sound", "%d channels, reverb %.2f", s->numChannels, s->reverb);
}

#pragma mark - TableViewDataSource

/**
 * @see TableViewDataSource::numberOfRows
 */
static size_t numberOfRows(const TableView *tableView) {
  return ((DiagnosticsView *) tableView)->numRows;
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

    $(table, addColumnWithIdentifier, _name);
    $(table, addColumnWithIdentifier, _value);

    table->dataSource.numberOfRows = numberOfRows;
    table->dataSource.self = this;

    table->delegate.cellForColumnAndRow = cellForColumnAndRow;
    table->delegate.self = this;

    $((View *) table->headerView, setVisibility, ViewVisibilityHidden);
  }

  return self;
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  DiagnosticsView *this = (DiagnosticsView *) self;

  $(self, setVisibility,
    cg_drawDiagnostics->integer ? ViewVisibilityVisible : ViewVisibilityHidden);

  if (data) {
    Client *cl = cgi.client;

    const uint32_t now = (uint32_t) SDL_GetTicks();

    if (self->visibility == ViewVisibilityHidden) {
      cl->packets = 0;
      this->frames = 0;
      this->time = now;
      return;
    }

    this->frames++;

    if (now - this->time >= 1000) {
      this->fps = this->frames;
      this->frames = 0;

      this->pps = cl->packets;
      cl->packets = 0;

      this->time = now;
    }

    if (now - this->refreshTime >= DIAGNOSTICS_REFRESH_INTERVAL) {
      this->refreshTime = now;

      refresh(this, (const ClientFrame *) data);
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
