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

#include "HomeViewController.h"

#define _Class _HomeViewController

#define QUETOO_STATS_URL "https://giblets.quetoo.org/api/stats"

static const char *_rank        = "Rank";
static const char *_player      = "Player";
static const char *_frags       = "Frags";
static const char *_deaths      = "Deaths";
static const char *_kd          = "KD";
static const char *_time_played = "Time";

/**
 * @brief Maps a column identifier to its API sort parameter.
 */
static const char *sortParamForColumn(const char *identifier) {
  if (g_strcmp0(identifier, _player)      == 0) return "name";
  if (g_strcmp0(identifier, _frags)       == 0) return "frags";
  if (g_strcmp0(identifier, _deaths)      == 0) return "deaths";
  if (g_strcmp0(identifier, _kd)          == 0) return "kd";
  if (g_strcmp0(identifier, _time_played) == 0) return "time_played";
  return NULL; /* Rank is not sortable */
}

static const JsonProperty leaderboard_properties[] = MakeJsonProperties(
  MakeJsonProperty(LeaderboardEntry, rank,     JsonPropertyInteger),
  MakeJsonProperty(LeaderboardEntry, name,     JsonPropertyString),
  MakeJsonProperty(LeaderboardEntry, guid,     JsonPropertyString),
  MakeJsonProperty(LeaderboardEntry, frags,    JsonPropertyInteger),
  MakeJsonProperty(LeaderboardEntry, deaths,   JsonPropertyInteger),
  MakeJsonProperty(LeaderboardEntry, damage,   JsonPropertyInteger),
  MakeJsonProperty(LeaderboardEntry, captures,    JsonPropertyInteger),
  MakeJsonProperty(LeaderboardEntry, time_played, JsonPropertyInteger)
);

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

#pragma mark - TableViewDataSource

/**
 * @see TableViewDataSource::numberOfRows
 */
static size_t numberOfRows(const TableView *tableView) {

  HomeViewController *this = tableView->dataSource.self;

  return this->num_entries;
}

#pragma mark - TableViewDelegate

/**
 * @see TableViewDelegate::cellForColumnAndRow
 */
static TableCellView *cellForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

  HomeViewController *this = tableView->dataSource.self;

  const LeaderboardEntry *entry = &this->entries[row];

  TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);

  if (row == 0) {
    $(((View *) cell), addClassName, "gold");
  } else if (row == 1) {
    $(((View *) cell), addClassName, "silver");
  } else if (row == 2) {
    $(((View *) cell), addClassName, "bronze");
  }

  if (g_strcmp0(column->identifier, _rank) == 0) {
    $(cell->text, setText, va("%d", entry->rank));
  } else if (g_strcmp0(column->identifier, _player) == 0) {
    $(cell->text, setText, entry->name);
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

  HomeViewController *this = tableView->delegate.self;

  void *body = NULL;
  size_t length = 0;

  const TableColumn *col = tableView->sortColumn;
  const char *sort = col ? sortParamForColumn(col->identifier) : NULL;
  const char *dir  = (col && col->order == OrderAscending) ? "asc" : "desc";

  char url[256];
  if (sort) {
    g_snprintf(url, sizeof(url), QUETOO_STATS_URL "?limit=%d&ai=0&sort=%s&dir=%s",
               LEADERBOARD_MAX_ENTRIES, sort, dir);
  } else {
    g_snprintf(url, sizeof(url), QUETOO_STATS_URL "?limit=%d&ai=0",
               LEADERBOARD_MAX_ENTRIES);
  }

  const int32_t status = cgi.HttpGet(url, &body, &length);
  if (status == 200) {
    Data *data = $$(Data, dataWithConstMemory, body, length);
    this->num_entries = $$(JSONSerialization, instancesFromData,
      leaderboard_properties, data, this->entries,
      sizeof(LeaderboardEntry), LEADERBOARD_MAX_ENTRIES);
    release(data);
    $(this->leaderboard, reloadData);
  } else {
    Cg_Warn("Failed to fetch leaderboard: HTTP %d\n", status);
  }

  cgi.Free(body);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  HomeViewController *this = (HomeViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("motd",        &this->motd),
    MakeOutlet("leaderboard", &this->leaderboard)
  );

  View *view = $$(View, viewWithResourceName, "ui/home/HomeViewController.json", outlets);
  assert(view);

  view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/home/HomeViewController.css");
  assert(view->stylesheet);

  $(self, setView, view);
  release(view);

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

  HomeViewController *this = (HomeViewController *) self;

  {
    void *body = NULL;
    size_t length = 0;

    const int32_t status = cgi.HttpGet(QUETOO_MOTD_URL, &body, &length);
    if (status == 200) {

      MutableString *motd = $(alloc(MutableString), initWithBytes, body, length, STRING_ENCODING_UTF8);
      $(motd, trim);

      Cg_Debug("Fetched motd: %s\n", motd->string.chars);

      $(this->motd->text, setText, motd->string.chars);

      release(motd);
    } else {
      Cg_Warn("Failed to fetch motd: HTTP %d\n", status);
    }

    cgi.Free(body);
  }

  {
    void *body = NULL;
    size_t length = 0;

    const int32_t status = cgi.HttpGet(QUETOO_STATS_URL "?limit=" G_STRINGIFY(LEADERBOARD_MAX_ENTRIES) "&ai=0", &body, &length);
    if (status == 200) {

      Data *data = $$(Data, dataWithConstMemory, body, length);
      this->num_entries = $$(JSONSerialization, instancesFromData,
        leaderboard_properties, data, this->entries,
        sizeof(LeaderboardEntry), LEADERBOARD_MAX_ENTRIES);
      release(data);

      $(this->leaderboard, reloadData);
    } else {
      Cg_Warn("Failed to fetch leaderboard: HTTP %d\n", status);
    }

    cgi.Free(body);
  }
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
 * @fn Class *HomeViewController::_HomeViewController(void)
 * @memberof HomeViewController
 */
Class *_HomeViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "HomeViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(HomeViewController),
      .interfaceOffset = offsetof(HomeViewController, interface),
      .interfaceSize = sizeof(HomeViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
