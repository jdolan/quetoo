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

#include <ObjectivelyMVC/ImageView.h>

#include "cg_local.h"

#include "HudViewController.h"
#include "IntermissionView.h"
#include "MapshotThumbnailView.h"

#define _Class _IntermissionView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  IntermissionView *this = (IntermissionView *) self;

  release(this->strip);
  release(this->result);
  release(this->prompt);
  release(this->maps);
  release(this->countdown);
  release(this->stats);

  super(Object, self, dealloc);
}

#pragma mark - Tiles

/**
 * @brief The map's mapshots, or a placeholder for a map this client does not have.
 * @remarks Resolving a mapshot enumerates the filesystem, so this runs only when the
 * candidates change, never per frame.
 */
static View *thumbnail(const char *map) {

  List *mapshots = cgi.Mapshots(map);
  const bool found = mapshots->count > 0;
  release(mapshots);

  if (found) {
    MapshotThumbnailView *view = $(alloc(MapshotThumbnailView), initWithFrame, NULL);
    assert(view);

    $(view, setMap, map);
    return (View *) view;
  }

  ImageView *view = $(alloc(ImageView), initWithFrame, NULL);
  assert(view);

  Image *image = Cg_LoadImage(va("ui/backgrounds/%u", (uint32_t) (Str_Length(map) % 6)));
  $(view, setImage, image);
  release(image);

  return (View *) view;
}

/**
 * @brief Appends the tile for the candidate at `index`.
 */
static void addMap(IntermissionView *self, int32_t index) {

  const CGameNextMapState *nextMap = &cgameState.nextMap;

  StackView *tile = $(alloc(StackView), initWithFrame, NULL);
  assert(tile);

  $((View *) tile, addClassName, "map");

  View *mapshot = thumbnail(nextMap->maps[index]);

  $(mapshot, addClassName, "mapshot");
  $((View *) tile, addSubview, mapshot);
  release(mapshot);

  // the key that picks it, which is the only way to pick one: the HUD layer is drawn
  // beneath the menus and never sees the mouse
  Text *name = $(alloc(Text), initWithText, nextMap->voting
                 ? va("%d  %s", index + 1, nextMap->maps[index])
                 : nextMap->maps[index], NULL);
  assert(name);

  $((View *) name, addClassName, "name");
  $((View *) tile, addSubview, (View *) name);
  release(name);

  if (nextMap->voting) {
    self->votes[index] = $(alloc(Text), initWithText, "", NULL);
    assert(self->votes[index]);

    $((View *) self->votes[index], addClassName, "votes");
    $((View *) tile, addSubview, (View *) self->votes[index]);
    release(self->votes[index]);
  }

  $((View *) self->maps, addSubview, (View *) tile);
  release(tile);
}

#pragma mark - Stats

/**
 * @brief Hits per shot as a percentage, or a dash for a weapon never fired.
 */
static const char *accuracyText(uint32_t hits, uint32_t shots) {
  return shots ? va("%.1f%%", hits * 100.f / shots) : "-";
}

/**
 * @brief Orders weapons by damage dealt, then by kills, most first.
 */
static int32_t weaponStatsComparator(const void *a, const void *b) {

  const CGameWeaponStats *wa = a, *wb = b;

  if (wa->damage != wb->damage) {
    return wa->damage > wb->damage ? -1 : 1;
  }

  return (int32_t) wb->kills - (int32_t) wa->kills;
}

/**
 * @brief Appends a Text with the given class name to the view.
 */
static void addText(View *view, const char *className, const char *text) {

  Text *label = $(alloc(Text), initWithText, text, NULL);
  assert(label);

  $((View *) label, addClassName, className);
  $(view, addSubview, (View *) label);
  release(label);
}

/**
 * @brief Appends a total: its value over its caption.
 */
static void addTotal(View *totals, const char *caption, const char *value) {

  StackView *total = $(alloc(StackView), initWithFrame, NULL);
  assert(total);

  $((View *) total, addClassName, "total");

  addText((View *) total, "value", value);
  addText((View *) total, "caption", caption);

  $(totals, addSubview, (View *) total);
  release(total);
}

/**
 * @brief Appends a row of cells to the weapon table.
 */
static void addWeapon(View *weapons, const char *icon, const char *name, const char *accuracy,
                      const char *damage, const char *kills) {

  StackView *row = $(alloc(StackView), initWithFrame, NULL);
  assert(row);

  $((View *) row, addClassName, "weapon");

  ImageView *image = $(alloc(ImageView), initWithFrame, NULL);
  assert(image);

  if (icon) {
    $(image, setImage, (Image *) Cg_HudImage(icon));
  }

  $((View *) image, addClassName, "icon");
  $((View *) row, addSubview, (View *) image);
  release(image);

  addText((View *) row, "weaponName", name);
  addText((View *) row, "accuracy", accuracy);
  addText((View *) row, "damage", damage);
  addText((View *) row, "kills", kills);

  $(weapons, addSubview, (View *) row);
  release(row);
}

#pragma mark - IntermissionView

/**
 * @fn void IntermissionView::rebuildStats(IntermissionView *self)
 * @memberof IntermissionView
 */
static void rebuildStats(IntermissionView *self) {

  const CGameMatchStats *stats = &cgameState.matchStats;

  $((View *) self->stats, removeAllSubviews);

  if (stats->numWeapons == 0 && stats->damageDealt == 0 && stats->damageTaken == 0) {
    $((View *) self->stats, setVisibility, ViewVisibilityHidden);
    return;
  }

  $((View *) self->stats, setVisibility, ViewVisibilityVisible);

  CGameWeaponStats weapons[WEAPON_TOTAL];
  memcpy(weapons, stats->weapons, sizeof(weapons));
  qsort(weapons, stats->numWeapons, sizeof(CGameWeaponStats), weaponStatsComparator);

  uint32_t shots = 0, hits = 0;
  for (int32_t i = 0; i < stats->numWeapons; i++) {
    shots += weapons[i].shots;
    hits += weapons[i].hits;
  }

  const bool favorite = stats->numWeapons && weapons[0].damage;

  StackView *totals = $(alloc(StackView), initWithFrame, NULL);
  assert(totals);

  $((View *) totals, addClassName, "totals");

  addTotal((View *) totals, "damage dealt", va("%u", stats->damageDealt));
  addTotal((View *) totals, "damage taken", va("%u", stats->damageTaken));
  addTotal((View *) totals, "accuracy", accuracyText(hits, shots));
  addTotal((View *) totals, "favorite", favorite ? gameItemDefs[weapons[0].tag].name : "-");

  $((View *) self->stats, addSubview, (View *) totals);
  release(totals);

  if (stats->numWeapons) {
    StackView *table = $(alloc(StackView), initWithFrame, NULL);
    assert(table);

    $((View *) table, addClassName, "weapons");

    addWeapon((View *) table, NULL, "weapon", "accuracy", "damage", "kills");

    for (int32_t i = 0; i < stats->numWeapons; i++) {
      const CGameWeaponStats *w = &weapons[i];
      addWeapon((View *) table, gameItemDefs[w->tag].icon, gameItemDefs[w->tag].name,
                accuracyText(w->hits, w->shots), va("%u", w->damage), va("%u", w->kills));
    }

    $((View *) self->stats, addSubview, (View *) table);
    release(table);
  }
}

/**
 * @fn void IntermissionView::rebuild(IntermissionView *self)
 * @memberof IntermissionView
 */
static void rebuild(IntermissionView *self) {

  $((View *) self->maps, removeAllSubviews);

  memset(self->votes, 0, sizeof(self->votes));

  for (int32_t i = 0; i < cgameState.nextMap.numMaps; i++) {
    addMap(self, i);
  }

  if (cgameState.nextMap.voting) {
    $(self->prompt, setText, va("Press 1 - %d to vote for the next map", cgameState.nextMap.numMaps));
    $((View *) self->prompt, setVisibility, ViewVisibilityVisible);
  } else if (cgameState.nextMap.practice) {
    $(self->prompt, setText, "2  Retry     3  Exit");
    $((View *) self->prompt, setVisibility, ViewVisibilityVisible);
  } else {
    $((View *) self->prompt, setVisibility, ViewVisibilityHidden);
  }

  self->practiceResult = -1;
  self->practiceScores = Cg_ScoresGeneration();

  $(self->result, setText, "");
  $((View *) self->result, setVisibility,
    cgameState.nextMap.practice ? ViewVisibilityVisible : ViewVisibilityHidden);
  $((View *) self->countdown, setVisibility,
    cgameState.nextMap.practice ? ViewVisibilityHidden : ViewVisibilityVisible);
}

/**
 * @brief Shows the result of a practice match, once the intermission's scores arrive, and
 * records a win.
 * @remarks The server shares one score cache for half a second, so the first scores of the
 * intermission can predate the frag that ended it. The second cannot.
 */
static void updatePractice(IntermissionView *self) {

  if (Cg_ScoresGeneration() - self->practiceScores < 2) {
    return;
  }

  const int32_t won = Cg_Practice_Won();
  if (won == self->practiceResult) {
    return;
  }

  self->practiceResult = won;

  $(self->result, setText, won ? "Victory" : "Defeat");

  if (won && Cg_Practice_Next()) {
    $(self->prompt, setText, "1  Next     2  Retry     3  Exit");
  } else {
    $(self->prompt, setText, "2  Retry     3  Exit");
  }

  if (won) {
    Cg_Practice_RecordWin();
  }
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = super(View, self, init);
  if (self) {
    IntermissionView *this = (IntermissionView *) self;

    this->strip = $(alloc(StackView), initWithFrame, NULL);
    assert(this->strip);

    $((View *) this->strip, addClassName, "strip");
    $(self, addSubview, (View *) this->strip);

    this->result = $(alloc(Text), initWithText, "", NULL);
    assert(this->result);

    $((View *) this->result, addClassName, "result");
    $((View *) this->strip, addSubview, (View *) this->result);
    $((View *) this->result, setVisibility, ViewVisibilityHidden);

    this->prompt = $(alloc(Text), initWithText, "", NULL);
    assert(this->prompt);

    $((View *) this->prompt, addClassName, "prompt");
    $((View *) this->strip, addSubview, (View *) this->prompt);
    $((View *) this->prompt, setVisibility, ViewVisibilityHidden);

    this->maps = $(alloc(StackView), initWithFrame, NULL);
    assert(this->maps);

    $((View *) this->maps, addClassName, "maps");
    $((View *) this->strip, addSubview, (View *) this->maps);

    this->countdown = $(alloc(Text), initWithText, "", NULL);
    assert(this->countdown);

    $((View *) this->countdown, addClassName, "countdown");
    $(self, addSubview, (View *) this->countdown);

    this->stats = $(alloc(StackView), initWithFrame, NULL);
    assert(this->stats);

    $((View *) this->stats, addClassName, "stats");
    $(self, addSubview, (View *) this->stats);
    $((View *) this->stats, setVisibility, ViewVisibilityHidden);
  }

  return self;
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  IntermissionView *this = (IntermissionView *) self;

  const CGameNextMapState *nextMap = &cgameState.nextMap;

  if (data) {

    // rebuilt before the recursion, so that new tiles take this frame too
    if (nextMap->generation != this->generation) {
      this->generation = nextMap->generation;

      $(this, rebuild);
    }

    if (cgameState.matchStats.generation != this->statsGeneration) {
      this->statsGeneration = cgameState.matchStats.generation;

      $(this, rebuildStats);
    }

    if (nextMap->practice) {
      updatePractice(this);
    }

    for (int32_t i = 0; i < nextMap->numMaps; i++) {
      if (this->votes[i]) {
        $(this->votes[i], setText, va("%d", nextMap->votes[i]));
      }
    }

    // the server publishes the intermission's clock here once the match clock stops
    const char *time = cgi.ConfigString(CS_TIME);
    if (!Str_CompareN(time, "^7", 2)) {
      time += 2;
    }

    $(this->countdown, setText, time);
  }

  super(View, self, updateBindings, data);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

  ((IntermissionViewInterface *) clazz->interface)->rebuild = rebuild;
  ((IntermissionViewInterface *) clazz->interface)->rebuildStats = rebuildStats;
}

/**
 * @fn Class *IntermissionView::_IntermissionView(void)
 * @memberof IntermissionView
 */
Class *_IntermissionView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "IntermissionView",
      .superclass = _View(),
      .instanceSize = sizeof(IntermissionView),
      .interfaceSize = sizeof(IntermissionViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
