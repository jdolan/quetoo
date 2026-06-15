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

#include <string.h>

#include "cg_local.h"

#include <Objectively/JSONContext.h>

#include "StatsViewController.h"

#define _Class _StatsViewController

#define QUETOO_STATS_URL "https://giblets.quetoo.org/api/stats"

static const char *_weapon = "Weapon";
static const char *_frags = "Frags";

#pragma mark - JSON deserialization

/**
 * @brief Formats a duration in seconds as "Xh Ym" or "Ym".
 */
static const char *formatTime(int32_t seconds) {
  if (seconds <= 0) {
    return "—";
  }

  const int32_t h = seconds / 3600;
  const int32_t m = (seconds % 3600) / 60;
  if (h > 0) {
    return va("%dh %dm", h, m);
  }

  return va("%dm", m);
}

static const JSONProperties nemesisProperties = MakeJSONProperties(Nemesis,
  MakeJSONProperty(Nemesis, name, NULL, JSONDeserializeCharacters, NULL)
);

static const JSONProperties killsByWeaponProperties = MakeJSONProperties(KillsByWeapon,
  MakeJSONProperty(KillsByWeapon, weapon, NULL, JSONDeserializeCharacters, NULL),
  MakeJSONProperty(KillsByWeapon, frags, NULL, JSONDeserializeInt32, NULL)
);

static const JSONArrayProperties killsByWeaponArrayProperties = {
  .properties = &killsByWeaponProperties,
  .capacity = lengthof(((StatsResponse *) 0)->kills_by_weapon),
  .count = JSONArrayProperties_NoCount
};

static const JSONProperties statsResponseProperties = MakeJSONProperties(StatsResponse,
  MakeJSONProperty(StatsResponse, rank, NULL, JSONDeserializeInt32, NULL),
  MakeJSONProperty(StatsResponse, frags, NULL, JSONDeserializeInt32, NULL),
  MakeJSONProperty(StatsResponse, deaths, NULL, JSONDeserializeInt32, NULL),
  MakeJSONProperty(StatsResponse, time_played, NULL, JSONDeserializeInt32, NULL),
  MakeJSONProperty(StatsResponse, nemesis, NULL, JSONDeserializeStruct, (ident) &nemesisProperties),
  MakeJSONProperty(StatsResponse, kills_by_weapon, NULL, JSONDeserializeArray, (ident) &killsByWeaponArrayProperties)
);

/**
 * @brief Staging buffer written by the HTTP thread, read by the main thread.
 * The SDL event queue provides the happens-before guarantee between the write
 * (before `SDL_PushEvent`) and the read (after `SDL_PollEvent`).
 */
static StatsResponse pendingStatsResponse;
static int32_t pendingStatsStatus;

/**
 * @brief `RESTClientCompletion` for `fetchStats`. Runs on the HTTP session thread;
 * hydrates `stats_pending` and dispatches `NOTIFICATION_STATS_FETCHED` as the signal.
 */
static void fetchStatsComplete(int32_t status, Data *data, void *user_data) {

  memset(&pendingStatsResponse, 0, sizeof(pendingStatsResponse));
  pendingStatsStatus = status;

  if (status == 200 && data) {
    JSONContext *ctx = $(alloc(JSONContext), init);
    $(ctx, structFromData, &statsResponseProperties, data, &pendingStatsResponse);
    release(ctx);
  } else if (status && status != 200) {
    Cg_Warn("Failed to fetch stats: HTTP %d\n", status);
  }

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_STATS_FETCHED
  });
}

/**
 * @brief Applies a decoded stats response to the view.
 */
static void loadStats(StatsViewController *this, int32_t status, const StatsResponse *s) {

  if (status == 200) {
    $(this->nameLabel->text, setText, cgi.GetCvarString("name"));
    $(this->rankLabel->text, setText, s->rank ? va("#%d", s->rank) : "—");
    $(this->fragsLabel->text, setText, s->frags ? va("%d", s->frags) : "—");
    $(this->deathsLabel->text, setText, s->deaths ? va("%d", s->deaths) : "—");

    const double kd = s->deaths > 0 ? (double) s->frags / s->deaths : (double) s->frags;
    $(this->kdLabel->text, setText, s->frags ? va("%.2f", kd) : "—");

    $(this->timeLabel->text, setText, formatTime(s->time_played));
    $(this->nemesisLabel->text, setText, s->nemesis.name[0] ? s->nemesis.name : "—");
  } else {
    $(this->rankLabel->text, setText, "—");
    $(this->fragsLabel->text, setText, "Error");
    $(this->deathsLabel->text, setText, "—");
    $(this->kdLabel->text, setText, "—");
    $(this->timeLabel->text, setText, "—");
  }

  $(this->weaponsTable, reloadData);
}

/**
 * @brief Clears the stats view and fires an asynchronous stats fetch for the local player.
 */
static void fetchStats(StatsViewController *this) {

  $(this->rankLabel->text, setText, "—");
  $(this->fragsLabel->text, setText, "—");
  $(this->deathsLabel->text, setText, "—");
  $(this->kdLabel->text, setText, "—");
  $(this->timeLabel->text, setText, "—");

  const char *guid_hash = cgi.GetCvarString("guid_hash");
  if (!guid_hash || !guid_hash[0]) {
    $(this->fragsLabel->text, setText, "Sign in");
    $(this->weaponsTable, reloadData);
    return;
  }

  $(this->weaponsTable, reloadData);

  char url[MAX_STRING_CHARS];
  g_snprintf(url, sizeof(url), QUETOO_STATS_URL "/%s", guid_hash);

  $(cgi.restClient, getAsync, url, fetchStatsComplete, NULL);
}

#pragma mark - TableViewDataSource

/**
 * @see TableViewDataSource::numberOfRows
 */
static size_t numberOfRows(const TableView *tableView) {

  StatsViewController *this = tableView->dataSource.self;

  size_t i;
  const KillsByWeapon *w = this->stats.kills_by_weapon;
  for (i = 0; i < lengthof(this->stats.kills_by_weapon); i++, w++) {
    if (strlen(w->weapon) == 0) {
      break;
    }
  }

  return i;
}

#pragma mark - TableViewDelegate

/**
 * @see TableViewDelegate::cellForColumnAndRow
 */
static TableCellView *cellForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

  StatsViewController *this = tableView->dataSource.self;

  const KillsByWeapon *w = &this->stats.kills_by_weapon[row];

  TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);

  if (g_strcmp0(column->identifier, _weapon) == 0) {
    $(cell->text, setText, w->weapon);
  } else if (g_strcmp0(column->identifier, _frags) == 0) {
    $(cell->text, setText, va("%d", w->frags));
  }

  return cell;
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  StatsViewController *this = (StatsViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("nameLabel",     &this->nameLabel),
    MakeOutlet("rankLabel",     &this->rankLabel),
    MakeOutlet("fragsLabel",    &this->fragsLabel),
    MakeOutlet("deathsLabel",   &this->deathsLabel),
    MakeOutlet("kdLabel",       &this->kdLabel),
    MakeOutlet("timeLabel",     &this->timeLabel),
    MakeOutlet("nemesisLabel",  &this->nemesisLabel),
    MakeOutlet("weapons",       &this->weaponsTable)
  );

  $(self->view, awakeWithResourceName, "ui/home/StatsViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/home/StatsViewController.css");
  assert(self->view->stylesheet);

  $(this->weaponsTable, addColumnWithIdentifier, _weapon);
  $(this->weaponsTable, addColumnWithIdentifier, _frags);

  this->weaponsTable->dataSource.numberOfRows = numberOfRows;
  this->weaponsTable->dataSource.self = this;

  this->weaponsTable->delegate.cellForColumnAndRow = cellForColumnAndRow;
  this->weaponsTable->delegate.self = this;
}

/**
 * @see ViewController::respondToEvent(ViewController *, const SDL_Event *)
 */
static void respondToEvent(ViewController *self, const SDL_Event *event) {

  StatsViewController *this = (StatsViewController *) self;

  if (event->type == MVC_NOTIFICATION_EVENT) {
    if (event->user.code == NOTIFICATION_STATS_FETCHED) {
      memcpy(&this->stats, &pendingStatsResponse, sizeof(this->stats));
      loadStats(this, pendingStatsStatus, &this->stats);
    }
  }

  super(ViewController, self, respondToEvent, event);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  super(ViewController, self, viewWillAppear);

  StatsViewController *this = (StatsViewController *) self;

  fetchStats(this);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {
  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->respondToEvent = respondToEvent;
  ((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;
}

/**
 * @fn Class *StatsViewController::_StatsViewController(void)
 * @memberof StatsViewController
 */
Class *_StatsViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "StatsViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(StatsViewController),
      .interfaceOffset = offsetof(StatsViewController, interface),
      .interfaceSize = sizeof(StatsViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
