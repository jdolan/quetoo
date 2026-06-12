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

#include <Objectively/JSONSerialization.h>
#include <Objectively/Null.h>
#include <Objectively/Number.h>
#include <Objectively/String.h>

#include "StatsViewController.h"

#define _Class _StatsViewController

#define QUETOO_STATS_URL "https://giblets.quetoo.org/api/stats"

typedef struct {
  int32_t rank;
  int32_t frags;
  int32_t deaths;
  int32_t time_played;
} StatsSummary;

static const JsonProperty stats_summary_properties[] = MakeJsonProperties(
  MakeJsonProperty(StatsSummary, rank,        JsonPropertyInteger),
  MakeJsonProperty(StatsSummary, frags,       JsonPropertyInteger),
  MakeJsonProperty(StatsSummary, deaths,      JsonPropertyInteger),
  MakeJsonProperty(StatsSummary, time_played, JsonPropertyInteger)
);

static const char *_weapon = "Weapon";
static const char *_frags  = "Frags";

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

/**
 * @brief Returns an integer value for the specified key path, or 0.
 */
static int32_t integerForKeyPath(const Dictionary *dict, const char *path) {

  const ident value = $(dict, objectForKeyPathWithClass, path, _Number());
  if (value) {
    const Number *number = cast(Number, value);
    return $(number, intValue);
  }

  return 0;
}

/**
 * @brief Clears cached stats data.
 */
static void clearStats(StatsViewController *this) {
  this->rank = 0;
  this->frags = 0;
  this->deaths = 0;
  this->time_played = 0;
  this->nemesis[0] = '\0';
  this->num_weapons = 0;
  memset(this->weapons, 0, sizeof(this->weapons));
}

/**
 * @brief Updates all tile labels from current stats fields.
 */
static void updateTiles(StatsViewController *this) {
  const double kd = this->deaths > 0 ? (double) this->frags / this->deaths : (double) this->frags;
  $(this->nameLabel->text,    setText, cgi.GetCvarString("name"));
  $(this->rankLabel->text,    setText, this->rank       ? va("#%d",  this->rank)   : "—");
  $(this->fragsLabel->text,   setText, this->frags      ? va("%d",   this->frags)  : "—");
  $(this->deathsLabel->text,  setText, this->deaths     ? va("%d",   this->deaths) : "—");
  $(this->kdLabel->text,      setText, this->frags      ? va("%.2f", kd)           : "—");
  $(this->timeLabel->text,    setText, formatTime(this->time_played));
  $(this->nemesisLabel->text, setText, this->nemesis[0] ? this->nemesis            : "—");
}

/**
 * @brief Loads weapon rows from the given JSON array.
 */
static void loadWeapons(StatsViewController *this, const Array *array) {

  this->num_weapons = 0;

  if (array == NULL) {
    return;
  }

  for (size_t i = 0; i < array->count && i < STATS_MAX_WEAPON_ROWS; i++) {
    const Dictionary *entry = cast(Dictionary, $(array, objectAtIndex, i));
    if (entry == NULL) {
      continue;
    }

    const String *weapon = $(entry, objectForKeyPathWithClass, "weapon", _String());
    if (weapon == NULL) {
      continue;
    }

    WeaponStat *stat = &this->weapons[this->num_weapons++];
    memset(stat, 0, sizeof(*stat));
    g_strlcpy(stat->weapon, weapon->chars, sizeof(stat->weapon));
    stat->frags = integerForKeyPath(entry, "frags");
  }
}

/**
 * @brief Pending stats fetch results, shared with the HTTP session thread.
 */
static struct {
  SDL_Mutex *lock;
  uint32_t generation;
  int32_t status;
  void *body;
  size_t length;
} stats_fetch;

/**
 * @brief Net_HttpCallback for `refresh`. Runs on the HTTP session thread, so the
 * response body is staged and marshaled to the main thread via an MVC notification.
 */
static void fetchStatsComplete(int32_t status, void *body, size_t length, void *user_data) {

  const uint32_t generation = (uint32_t) (uintptr_t) user_data;

  SDL_LockMutex(stats_fetch.lock);

  if (generation != stats_fetch.generation) {
    SDL_UnlockMutex(stats_fetch.lock);
    return;
  }

  g_free(stats_fetch.body);

  stats_fetch.status = status;
  stats_fetch.body = NULL;
  stats_fetch.length = 0;

  if (body && length) {
    stats_fetch.body = g_malloc(length);
    memcpy(stats_fetch.body, body, length);
    stats_fetch.length = length;
  }

  SDL_UnlockMutex(stats_fetch.lock);

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_STATS_FETCHED
  });
}

/**
 * @brief Loads stats from the given response body, updating all tiles and tables.
 */
static void loadStats(StatsViewController *this, int32_t status, void *body, size_t length) {

  if (status == 200) {
    Data *data = $$(Data, dataWithConstMemory, body, length);
    StatsSummary summary = { 0 };
    (void) $$(JSONSerialization, instanceFromData, stats_summary_properties, data, &summary);
    ident json = $$(JSONSerialization, objectFromData, data, 0);
    release(data);

    if (json) {
      Dictionary *dict = cast(Dictionary, json);
      if (dict) {
        this->rank = summary.rank;
        this->frags = summary.frags;
        this->deaths = summary.deaths;
        this->time_played = summary.time_played;

        const Dictionary *nemesis = $(dict, objectForKeyPathWithClass, "nemesis", _Dictionary());
        if (nemesis) {
          const String *name = $(nemesis, objectForKeyPathWithClass, "name", _String());
          if (name) {
            g_strlcpy(this->nemesis, name->chars, sizeof(this->nemesis));
          }
        }

        loadWeapons(this, $(dict, objectForKeyPathWithClass, "kills_by_weapon", _Array()));

      } else if (!cast(Null, json)) {
        Cg_Warn("Unexpected stats response type: %s\n", classnameof(json));
      }

      updateTiles(this);
      release(json);
    } else {
      this->rank = summary.rank;
      this->frags = summary.frags;
      this->deaths = summary.deaths;
      this->time_played = summary.time_played;
      updateTiles(this);
    }
  } else {
    $(this->rankLabel->text, setText, "—");
    $(this->fragsLabel->text, setText, "Error");
    $(this->deathsLabel->text, setText, "—");
    $(this->kdLabel->text, setText, "—");
    $(this->timeLabel->text, setText, "—");
    Cg_Warn("Failed to fetch stats: HTTP %d\n", status);
  }

  $(this->weaponsTable, reloadData);
}

/**
 * @brief Clears the view and asynchronously fetches stats for the local player.
 */
static void refresh(StatsViewController *this) {

  clearStats(this);

  const char *guid_hashed = cgi.GetCvarString("guid_hashed");

  if (!guid_hashed || !guid_hashed[0]) {
    $(this->rankLabel->text, setText, "—");
    $(this->fragsLabel->text, setText, "Sign in");
    $(this->deathsLabel->text, setText, "—");
    $(this->kdLabel->text, setText, "—");
    $(this->timeLabel->text, setText, "—");
    $(this->weaponsTable, reloadData);
    return;
  }

  updateTiles(this);
  $(this->weaponsTable, reloadData);

  char url[256];
  g_snprintf(url, sizeof(url), QUETOO_STATS_URL "/%s", guid_hashed);

  SDL_LockMutex(stats_fetch.lock);
  const uint32_t generation = ++stats_fetch.generation;
  SDL_UnlockMutex(stats_fetch.lock);

  cgi.HttpGetAsync(url, fetchStatsComplete, (void *) (uintptr_t) generation);
}

#pragma mark - TableViewDataSource

/**
 * @see TableViewDataSource::numberOfRows
 */
static size_t numberOfRows(const TableView *tableView) {

  StatsViewController *this = tableView->dataSource.self;

  return this->num_weapons;
}

#pragma mark - TableViewDelegate

/**
 * @see TableViewDelegate::cellForColumnAndRow
 */
static TableCellView *cellForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

  StatsViewController *this = tableView->dataSource.self;

  const WeaponStat *weapon = &this->weapons[row];

  TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);

  if (g_strcmp0(column->identifier, _weapon) == 0) {
    $(cell->text, setText, weapon->weapon);
  } else if (g_strcmp0(column->identifier, _frags) == 0) {
    $(cell->text, setText, va("%d", weapon->frags));
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
    MakeOutlet("nameLabel",    &this->nameLabel),
    MakeOutlet("rankLabel",    &this->rankLabel),
    MakeOutlet("fragsLabel",   &this->fragsLabel),
    MakeOutlet("deathsLabel",  &this->deathsLabel),
    MakeOutlet("kdLabel",      &this->kdLabel),
    MakeOutlet("timeLabel",    &this->timeLabel),
    MakeOutlet("nemesisLabel", &this->nemesisLabel),
    MakeOutlet("weapons",      &this->weaponsTable)
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

  clearStats(this);
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
      void *body = stats_fetch.body;
      const size_t length = stats_fetch.length;
      stats_fetch.body = NULL;
      stats_fetch.length = 0;
      SDL_UnlockMutex(stats_fetch.lock);

      loadStats(this, status, body, length);

      g_free(body);

    } else if (event->user.code == NOTIFICATION_GUID_HASHED) {
      refresh(this);
    }
  }

  super(ViewController, self, respondToEvent, event);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  super(ViewController, self, viewWillAppear);

  refresh((StatsViewController *) self);
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
