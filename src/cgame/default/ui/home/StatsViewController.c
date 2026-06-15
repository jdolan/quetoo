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
#include <string.h>

#include "cg_local.h"

#include <Objectively/JSONContext.h>
#include <SDL3/SDL_mutex.h>

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

static const JSONProperties nemesis_properties = MakeJSONProperties(Nemesis,
  MakeJSONProperty(Nemesis, name, NULL, JSONDeserializeCharacters, NULL)
);

static const JSONProperties kills_by_weapon_properties = MakeJSONProperties(KillsByWeapon,
  MakeJSONProperty(KillsByWeapon, weapon, NULL, JSONDeserializeCharacters, NULL),
  MakeJSONProperty(KillsByWeapon, frags, NULL, JSONDeserializeInt32, NULL)
);

static const JSONArrayProperties kills_by_weapon_array = {
  .properties   = &kills_by_weapon_properties,
  .capacity = lengthof(((StatsResponse *) 0)->kills_by_weapon),
  .count = JSONArrayProperties_NoCount
};

static const JSONProperties stats_properties = MakeJSONProperties(StatsResponse,
  MakeJSONProperty(StatsResponse, rank, NULL, JSONDeserializeInt32, NULL),
  MakeJSONProperty(StatsResponse, frags, NULL, JSONDeserializeInt32, NULL),
  MakeJSONProperty(StatsResponse, deaths, NULL, JSONDeserializeInt32, NULL),
  MakeJSONProperty(StatsResponse, time_played, NULL, JSONDeserializeInt32, NULL),
  MakeJSONProperty(StatsResponse, nemesis, NULL, JSONDeserializeStruct, (ident) &nemesis_properties),
  MakeJSONProperty(StatsResponse, kills_by_weapon, NULL, JSONDeserializeArray, (ident) &kills_by_weapon_array)
);

/**
 * @brief Pending stats fetch state, shared between the HTTP thread and the main thread.
 */
static struct {
  SDL_Mutex *lock;
  uint32_t generation;
  int32_t status;
  Data *data;
} stats_fetch;

/**
 * @brief `RESTClientCompletion` for `fetchStats`. Runs on the HTTP session thread;
 * retains `data` and marshals the result to the main thread via `NOTIFICATION_STATS_FETCHED`.
 */
static void fetchStatsComplete(int32_t status, Data *data, void *user_data) {

  const uint32_t generation = (uint32_t) (uintptr_t) user_data;

  SDL_LockMutex(stats_fetch.lock);

  if (generation == stats_fetch.generation) {
    stats_fetch.status = status;
    release(stats_fetch.data);
    stats_fetch.data = (status == 200 && data) ? retain(data) : NULL;
  }

  SDL_UnlockMutex(stats_fetch.lock);

  if (generation == stats_fetch.generation) {
    SDL_PushEvent(&(SDL_Event) {
      .user.type = MVC_NOTIFICATION_EVENT,
      .user.code = NOTIFICATION_STATS_FETCHED
    });
  }
}

/**
 * @brief Applies a completed stats response to the view. Must run on the main thread.
 */
static void loadStats(StatsViewController *this, int32_t status, Data *data) {

  StatsResponse *s = &this->stats;
  memset(s, 0, sizeof(*s));

  if (status == 200 && data) {
    JSONContext *ctx = $(alloc(JSONContext), init);
    $(ctx, structFromData, &stats_properties, data, s);
    release(ctx);

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
    $(this->fragsLabel->text, setText, status ? "Error" : "—");
    $(this->deathsLabel->text, setText, "—");
    $(this->kdLabel->text, setText, "—");
    $(this->timeLabel->text, setText, "—");
    if (status && status != 200) {
      Cg_Warn("Failed to fetch stats: HTTP %d\n", status);
    }
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

  const char *guid_hashed = cgi.GetCvarString("guid_hashed");
  if (!guid_hashed || !guid_hashed[0]) {
    $(this->fragsLabel->text, setText, "Sign in");
    $(this->weaponsTable, reloadData);
    return;
  }

  $(this->weaponsTable, reloadData);

  char url[MAX_STRING_CHARS];
  {
    time_t t = time(NULL);
    const struct tm *lt = localtime(&t);

    char from[16], to[16];
    snprintf(from, sizeof(from), "%04d-%02d-01", lt->tm_year + 1900, lt->tm_mon + 1);
    strftime(to, sizeof(to), "%Y-%m-%d", lt);

    g_snprintf(url, sizeof(url), QUETOO_STATS_URL "/%s?from=%s&to=%s", guid_hashed, from, to);
  }

  SDL_LockMutex(stats_fetch.lock);
  const uint32_t generation = ++stats_fetch.generation;
  SDL_UnlockMutex(stats_fetch.lock);

  $(cgi.restClient, getAsync, url, fetchStatsComplete, (void *) (uintptr_t) generation);
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

  if (stats_fetch.lock == NULL) {
    stats_fetch.lock = SDL_CreateMutex();
  }

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

      SDL_LockMutex(stats_fetch.lock);
      const int32_t status = stats_fetch.status;
      Data *data = stats_fetch.data ? retain(stats_fetch.data) : NULL;
      SDL_UnlockMutex(stats_fetch.lock);

      loadStats(this, status, data);
      release(data);

    } else if (event->user.code == NOTIFICATION_GUID_HASHED) {
      fetchStats(this);
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
