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

#include "cg_local.h"

#include <Objectively/JSONContext.h>

#include "LeaderboardViewController.h"

#define _Class _LeaderboardViewController

#define QUETOO_STATS_URL "https://giblets.quetoo.org/api/stats"

static const char *_rank        = "Rank";
static const char *_player      = "Player";
static const char *_frags       = "Frags";
static const char *_deaths      = "Deaths";
static const char *_kd          = "KD";
static const char *_time_played = "Time";

static const JSONProperties leaderboard_entry_properties = MakeJSONProperties(LeaderboardEntry,
  MakeJSONProperty(LeaderboardEntry, rank,        NULL, JSONDeserializeInt32,      NULL),
  MakeJSONProperty(LeaderboardEntry, name,        NULL, JSONDeserializeCharacters, NULL),
  MakeJSONProperty(LeaderboardEntry, guid,        NULL, JSONDeserializeCharacters, NULL),
  MakeJSONProperty(LeaderboardEntry, frags,       NULL, JSONDeserializeInt32,      NULL),
  MakeJSONProperty(LeaderboardEntry, deaths,      NULL, JSONDeserializeInt32,      NULL),
  MakeJSONProperty(LeaderboardEntry, captures,    NULL, JSONDeserializeInt32,      NULL),
  MakeJSONProperty(LeaderboardEntry, time_played, NULL, JSONDeserializeInt32,      NULL)
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
 * @brief Computes YYYY-MM-DD date strings for the given period.
 *        Sets from to empty string for "all time" (no date filter).
 */
static void periodDateRange(const char *period, char *from, size_t from_len, char *to, size_t to_len) {
  from[0] = '\0';
  to[0]   = '\0';

  if (!period || !period[0]) {
    return;
  }

  const time_t now = time(NULL);
  struct tm t = *localtime(&now);

  strftime(to, to_len, "%Y-%m-%d", &t);

  if (g_strcmp0(period, "week") == 0) {
    t.tm_mday -= t.tm_wday;
  } else if (g_strcmp0(period, "month") == 0) {
    t.tm_mday = 1;
  } else if (g_strcmp0(period, "year") == 0) {
    t.tm_mday = 1;
    t.tm_mon  = 0;
  } else {
    return;
  }

  mktime(&t);
  strftime(from, from_len, "%Y-%m-%d", &t);
}

/**
 * @brief Staging buffer written by the HTTP thread, read by the main thread.
 * The SDL event queue provides the happens-before guarantee between the write
 * (before SDL_PushEvent) and the read (after SDL_PollEvent).
 */
static LeaderboardResponse leaderboard_pending;

/**
 * @brief `RESTClientCompletion` for `fetchLeaderboard`. Runs on the HTTP session thread;
 * hydrates `leaderboard_pending` and dispatches `NOTIFICATION_LEADERBOARD_FETCHED` as the signal.
 */
static void fetchLeaderboardComplete(int32_t status, Data *data, void *user_data) {

  memset(&leaderboard_pending, 0, sizeof(leaderboard_pending));

  if (status == 200 && data) {
    JSONContext *ctx = $(alloc(JSONContext), init);
    leaderboard_pending.num_entries = $(ctx, structsFromData, &leaderboard_entry_properties, data,
                                        leaderboard_pending.entries, LEADERBOARD_MAX_ENTRIES);
    release(ctx);
  } else if (status && status != 200) {
    Cg_Warn("Failed to fetch leaderboard: HTTP %d\n", status);
  }

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_LEADERBOARD_FETCHED
  });
}

/**
 * @brief Fires an asynchronous leaderboard fetch using the given sort column.
 */
static void fetchLeaderboard(LeaderboardViewController *this, const TableColumn *column) {

  const char *sort = column ? sortParamForColumn(column->identifier) : NULL;
  const char *dir  = (column && column->order == OrderAscending) ? "asc" : "desc";

  char from[16] = {0}, to[16] = {0};
  periodDateRange("month", from, sizeof(from), to, sizeof(to));

  char url[512];
  int n = g_snprintf(url, sizeof(url), QUETOO_STATS_URL "?limit=%d&ai=0", LEADERBOARD_MAX_ENTRIES);
  if (sort) {
    n += g_snprintf(url + n, sizeof(url) - n, "&sort=%s&dir=%s", sort, dir);
  }
  if (from[0]) {
    n += g_snprintf(url + n, sizeof(url) - n, "&from=%s&to=%s", from, to);
  }

  $(cgi.restClient, getAsync, url, fetchLeaderboardComplete, NULL);
}

/**
 * @brief Selects the current player in the leaderboard, if present.
 */
static void selectOwnRow(LeaderboardViewController *this) {

  const char *guid_hashed = cgi.GetCvarString("guid_hashed");
  if (!guid_hashed || !guid_hashed[0]) {
    return;
  }

  for (size_t i = 0; i < this->leaderboard_response.num_entries; i++) {
    if (g_strcmp0(this->leaderboard_response.entries[i].guid, guid_hashed) == 0) {
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

  return this->leaderboard_response.num_entries;
}

#pragma mark - TableViewDelegate

/**
 * @see TableViewDelegate::cellForColumnAndRow
 */
static TableCellView *cellForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

  LeaderboardViewController *this = tableView->dataSource.self;

  const LeaderboardEntry *entry = &this->leaderboard_response.entries[row];

  TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);

  if (row == 0) {
    $((View *) cell, addClassName, "gold");
  } else if (row == 1) {
    $((View *) cell, addClassName, "silver");
  } else if (row == 2) {
    $((View *) cell, addClassName, "bronze");
  }

  const char *guid_hashed = cgi.GetCvarString("guid_hashed");
  if (guid_hashed && guid_hashed[0] && g_strcmp0(entry->guid, guid_hashed) == 0) {
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
 * @see ViewController::respondToEvent(ViewController *, const SDL_Event *)
 */
static void respondToEvent(ViewController *self, const SDL_Event *event) {

  LeaderboardViewController *this = (LeaderboardViewController *) self;

  if (event->type == MVC_NOTIFICATION_EVENT) {
    if (event->user.code == NOTIFICATION_LEADERBOARD_FETCHED) {

      this->leaderboard_response = leaderboard_pending;

      $(this->leaderboard, reloadData);
      selectOwnRow(this);

    } else if (event->user.code == NOTIFICATION_GUID_HASHED) {
      $(this->leaderboard, reloadData);
      selectOwnRow(this);
    }
  }

  super(ViewController, self, respondToEvent, event);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  super(ViewController, self, viewWillAppear);

  LeaderboardViewController *this = (LeaderboardViewController *) self;

  fetchLeaderboard(this, this->leaderboard->sortColumn);
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
