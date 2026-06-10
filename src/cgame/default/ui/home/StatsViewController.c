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

#define QUETOO_GUID_URL  "https://giblets.quetoo.org/api/guid"
#define QUETOO_STATS_URL "https://giblets.quetoo.org/api/stats"

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
static int32_t integerForKeyPath(const Dictionary *dict, const char *keyPath) {
  const Number *number = dict ? cast(Number, $(dict, objectForKeyPath, keyPath)) : NULL;
  return number ? $(number, intValue) : 0;
}

/**
 * @brief Returns the dictionary value at the specified key path, or `NULL`.
 */
static const Dictionary *dictionaryForKeyPath(const Dictionary *dict, const char *keyPath) {
  return dict ? cast(Dictionary, $(dict, objectForKeyPath, keyPath)) : NULL;
}

/**
 * @brief Returns the array value at the specified key path, or `NULL`.
 */
static const Array *arrayForKeyPath(const Dictionary *dict, const char *keyPath) {
  return dict ? cast(Array, $(dict, objectForKeyPath, keyPath)) : NULL;
}

/**
 * @brief Returns the string value at the specified key path, or `NULL`.
 */
static const String *stringForKeyPath(const Dictionary *dict, const char *keyPath) {
  return dict ? cast(String, $(dict, objectForKeyPath, keyPath)) : NULL;
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
 * @brief Fetches the hashed GUID for the current player.
 */
static bool fetchGuid(char *guid, size_t length) {

  guid[0] = '\0';

  const cvar_t *guid_cvar = cgi.GetCvar("guid");
  if (guid_cvar == NULL || guid_cvar->string[0] == '\0') {
    return false;
  }

  void *body = NULL;
  size_t body_length = 0;

  char url[256];
  g_snprintf(url, sizeof(url), QUETOO_GUID_URL "?guid=%s", guid_cvar->string);

  const int32_t status = cgi.HttpGet(url, &body, &body_length);
  if (status == 200) {
    Data *data = $$(Data, dataWithConstMemory, body, body_length);
    ident json = $$(JSONSerialization, objectFromData, data, 0);
    release(data);

    if (json) {
      Dictionary *dict = cast(Dictionary, json);
      if (dict) {
        const String *hashed = cast(String, $(dict, objectForKeyPath, "guid"));
        if (hashed) {
          g_strlcpy(guid, hashed->chars, length);
        }
      } else if (!cast(Null, json)) {
        Cg_Warn("Unexpected guid response type: %s\n", classnameof(json));
      }

      release(json);
    }
  } else {
    Cg_Warn("Failed to fetch guid: HTTP %d\n", status);
  }

  cgi.Free(body);

  return guid[0] != '\0';
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

    const String *weapon = stringForKeyPath(entry, "weapon");
    if (weapon == NULL) {
      continue;
    }

    WeaponStat *stat = &this->weapons[this->num_weapons++];
    memset(stat, 0, sizeof(*stat));
    g_strlcpy(stat->weapon, weapon->chars, sizeof(stat->weapon));
    stat->frags = integerForKeyPath(entry, "frags");
  }
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
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  super(ViewController, self, viewWillAppear);

  StatsViewController *this = (StatsViewController *) self;

  clearStats(this);

  char guid[68];
  if (!fetchGuid(guid, sizeof(guid))) {
    $(this->rankLabel->text, setText, "—");
    $(this->fragsLabel->text, setText, "Sign in");
    $(this->deathsLabel->text, setText, "—");
    $(this->kdLabel->text, setText, "—");
    $(this->timeLabel->text, setText, "—");
    $(this->weaponsTable, reloadData);
    return;
  }

  void *body = NULL;
  size_t length = 0;

  char url[256];
  g_snprintf(url, sizeof(url), QUETOO_STATS_URL "/%s", guid);

  const int32_t status = cgi.HttpGet(url, &body, &length);
  if (status == 200) {
    Data *data = $$(Data, dataWithConstMemory, body, length);
    ident json = $$(JSONSerialization, objectFromData, data, 0);
    release(data);

    if (json) {
      Dictionary *dict = cast(Dictionary, json);
      if (dict) {
        this->rank = integerForKeyPath(dict, "rank");
        this->frags = integerForKeyPath(dict, "frags");
        this->deaths = integerForKeyPath(dict, "deaths");
        this->time_played = integerForKeyPath(dict, "time_played");

        const Dictionary *nemesis = dictionaryForKeyPath(dict, "nemesis");
        if (nemesis) {
          const String *name = stringForKeyPath(nemesis, "name");
          if (name) {
            g_strlcpy(this->nemesis, name->chars, sizeof(this->nemesis));
          }
        }

        loadWeapons(this, arrayForKeyPath(dict, "kills_by_weapon"));
      } else if (!cast(Null, json)) {
        Cg_Warn("Unexpected stats response type: %s\n", classnameof(json));
      }

      updateTiles(this);
      release(json);
    } else {
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

  cgi.Free(body);

  $(this->weaponsTable, reloadData);
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
