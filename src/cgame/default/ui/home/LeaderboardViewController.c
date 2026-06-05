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

#include "LeaderboardViewController.h"

#define _Class _LeaderboardViewController

#define QUETOO_STATS_URL "https://giblets.quetoo.org/api/stats"
#define QUETOO_GUID_URL  "https://giblets.quetoo.org/api/guid"

static const char *_rank        = "Rank";
static const char *_player      = "Player";
static const char *_frags       = "Frags";
static const char *_deaths      = "Deaths";
static const char *_kd          = "KD";
static const char *_time_played = "Time";

static const JsonProperty leaderboard_properties[] = MakeJsonProperties(
  MakeJsonProperty(LeaderboardEntry, rank,        JsonPropertyInteger),
  MakeJsonProperty(LeaderboardEntry, name,        JsonPropertyString),
  MakeJsonProperty(LeaderboardEntry, guid,        JsonPropertyString),
  MakeJsonProperty(LeaderboardEntry, frags,       JsonPropertyInteger),
  MakeJsonProperty(LeaderboardEntry, deaths,      JsonPropertyInteger),
  MakeJsonProperty(LeaderboardEntry, damage,      JsonPropertyInteger),
  MakeJsonProperty(LeaderboardEntry, captures,    JsonPropertyInteger),
  MakeJsonProperty(LeaderboardEntry, time_played, JsonPropertyInteger)
);

/**
 * @brief Maps a column identifier to its API sort parameter.
 */
static const char *sortParamForColumn(const char *identifier) {
  if (g_strcmp0(identifier, _player)      == 0) return "name";
  if (g_strcmp0(identifier, _frags)       == 0) return "frags";
  if (g_strcmp0(identifier, _deaths)      == 0) return "deaths";
  if (g_strcmp0(identifier, _kd)          == 0) return "kd";
  if (g_strcmp0(identifier, _time_played) == 0) return "time_played";
  return NULL;
}

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
 * @brief Fetches and stores the hashed GUID for the current player.
 */
static void fetchGuid(LeaderboardViewController *this) {

  this->guid[0] = '\0';

  const cvar_t *guid_cvar = cgi.GetCvar("guid");
  if (guid_cvar == NULL || guid_cvar->string[0] == '\0') {
    return;
  }

  void *body = NULL;
  size_t length = 0;

  char url[256];
  g_snprintf(url, sizeof(url), QUETOO_GUID_URL "?guid=%s", guid_cvar->string);

  const int32_t status = cgi.HttpGet(url, &body, &length);
  if (status == 200) {
    Data *data = $$(Data, dataWithConstMemory, body, length);
    Dictionary *dict = (Dictionary *) $$(JSONSerialization, objectFromData, data, 0);
    release(data);

    if (dict) {
      const String *hashed = $(dict, objectForKeyPath, "guid");
      if (hashed) {
        g_strlcpy(this->guid, hashed->chars, sizeof(this->guid));
        Cg_Debug("Fetched hashed guid: %s\n", this->guid);
      }
      release(dict);
    }
  } else {
    Cg_Warn("Failed to fetch guid: HTTP %d\n", status);
  }

  cgi.Free(body);
}

/**
 * @brief Fetches leaderboard rows using the given sort column.
 */
static void fetchLeaderboard(LeaderboardViewController *this, const TableColumn *column) {

  void *body = NULL;
  size_t length = 0;

  const char *sort = column ? sortParamForColumn(column->identifier) : NULL;
  const char *dir  = (column && column->order == OrderAscending) ? "asc" : "desc";

  char url[256];
  if (sort) {
    g_snprintf(url, sizeof(url), QUETOO_STATS_URL "?limit=%d&ai=0&sort=%s&dir=%s", LEADERBOARD_MAX_ENTRIES, sort, dir);
  } else {
    g_snprintf(url, sizeof(url), QUETOO_STATS_URL "?limit=%d&ai=0", LEADERBOARD_MAX_ENTRIES);
  }

  const int32_t status = cgi.HttpGet(url, &body, &length);
  if (status == 200) {
    Data *data = $$(Data, dataWithConstMemory, body, length);
    this->num_entries = $$(JSONSerialization, instancesFromData,
      leaderboard_properties, data, this->entries,
      sizeof(LeaderboardEntry), LEADERBOARD_MAX_ENTRIES);
    release(data);
  } else {
    this->num_entries = 0;
    Cg_Warn("Failed to fetch leaderboard: HTTP %d\n", status);
  }

  cgi.Free(body);
}

/**
 * @brief Selects the current player in the leaderboard, if present.
 */
static void selectOwnRow(LeaderboardViewController *this) {

  if (this->guid[0] == '\0') {
    return;
  }

  for (size_t i = 0; i < this->num_entries; i++) {
    if (g_strcmp0(this->entries[i].guid, this->guid) == 0) {
      $(this->leaderboard, selectRowAtIndex, i);
      return;
    }
  }
}

#pragma mark - TableViewDataSource

/**
 * @see TableViewDataSource::numberOfRows
 */
static size_t numberOfRows(const TableView *tableView) {

  LeaderboardViewController *this = tableView->dataSource.self;

  return this->num_entries;
}

#pragma mark - TableViewDelegate

/**
 * @see TableViewDelegate::cellForColumnAndRow
 */
static TableCellView *cellForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

  LeaderboardViewController *this = tableView->dataSource.self;

  const LeaderboardEntry *entry = &this->entries[row];

  TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);

  if (row == 0) {
    $((View *) cell, addClassName, "gold");
  } else if (row == 1) {
    $((View *) cell, addClassName, "silver");
  } else if (row == 2) {
    $((View *) cell, addClassName, "bronze");
  }

  if (this->guid[0] && g_strcmp0(entry->guid, this->guid) == 0) {
    $((View *) cell, addClassName, "me");
  }

  if (g_strcmp0(column->identifier, _rank) == 0) {
    $(cell->text, setText, va("%d", entry->rank));
  } else if (g_strcmp0(column->identifier, _player) == 0) {
    $(cell->text, setText, entry->name);
    cell->text->colorEscapes = true;
  } else if (g_strcmp0(column->identifier, _frags) == 0) {
    $(cell->text, setText, va("%d", entry->frags));
  } else if (g_strcmp0(column->identifier, _deaths) == 0) {
    $(cell->text, setText, va("%d", entry->deaths));
  } else if (g_strcmp0(column->identifier, _kd) == 0) {
    const float kd = entry->deaths > 0 ? (float) entry->frags / entry->deaths : (float) entry->frags;
    $(cell->text, setText, va("%.2f", kd));
  } else if (g_strcmp0(column->identifier, _time_played) == 0) {
    $(cell->text, setText, formatTime(entry->time_played));
  }

  return cell;
}

/**
 * @see TableViewDelegate::didSetSortColumn
 */
static void didSetSortColumn(TableView *tableView) {

  LeaderboardViewController *this = tableView->delegate.self;

  fetchLeaderboard(this, tableView->sortColumn);
  $(this->leaderboard, reloadData);
  selectOwnRow(this);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  LeaderboardViewController *this = (LeaderboardViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("leaderboard", &this->leaderboard)
  );

  $(self->view, awakeWithResourceName, "ui/home/LeaderboardViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/home/LeaderboardViewController.css");
  assert(self->view->stylesheet);

  $(this->leaderboard, addColumnWithIdentifier, _rank);
  $(this->leaderboard, addColumnWithIdentifier, _player);
  $(this->leaderboard, addColumnWithIdentifier, _frags);
  $(this->leaderboard, addColumnWithIdentifier, _deaths);
  $(this->leaderboard, addColumnWithIdentifier, _kd);
  $(this->leaderboard, addColumnWithIdentifier, _time_played);

  this->leaderboard->dataSource.numberOfRows = numberOfRows;
  this->leaderboard->dataSource.self = this;

  this->leaderboard->delegate.cellForColumnAndRow = cellForColumnAndRow;
  this->leaderboard->delegate.didSetSortColumn = didSetSortColumn;
  this->leaderboard->delegate.self = this;
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  super(ViewController, self, viewWillAppear);

  LeaderboardViewController *this = (LeaderboardViewController *) self;

  fetchGuid(this);
  fetchLeaderboard(this, this->leaderboard->sortColumn);
  $(this->leaderboard, reloadData);
  selectOwnRow(this);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {
  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;
}

/**
 * @fn Class *LeaderboardViewController::_LeaderboardViewController(void)
 * @memberof LeaderboardViewController
 */
Class *_LeaderboardViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "LeaderboardViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(LeaderboardViewController),
      .interfaceOffset = offsetof(LeaderboardViewController, interface),
      .interfaceSize = sizeof(LeaderboardViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
