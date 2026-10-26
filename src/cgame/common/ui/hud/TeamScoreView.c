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

#include "TeamScoreView.h"

#define _Class _TeamScoreView

#pragma mark - View

/**
 * @see View::awakeWithDictionary(View *, const Dictionary *)
 */
static void awakeWithDictionary(View *self, const Dictionary *dictionary) {

  super(View, self, awakeWithDictionary, dictionary);

  TeamScoreView *this = (TeamScoreView *) self;

  const Inlet inlets[] = MakeInlets(
    MakeInlet("team", InletTypeInteger, &this->team, NULL)
  );

  $(self, bind, inlets, dictionary);
}

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  return (View *) $((CounterView *) self, initWithCaption, NULL, COUNTER_VIEW_NO_STAT);
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  if (data == NULL) {
    super(View, self, updateBindings, data);
    return;
  }

  TeamScoreView *this = (TeamScoreView *) self;
  CounterView *counter = (CounterView *) self;

  const int32_t numTeams = cgameState.numTeams;
  const int32_t facing = (this->team & 1) ? this->team + 1 : this->team - 1;

  this->plays = this->team > 0 && this->team <= numTeams;
  const bool holds = this->plays || (facing > 0 && facing <= numTeams);

  $(self, setVisibility, holds ? ViewVisibilityVisible : ViewVisibilityHidden);

  this->state[0] = '\0';

  if (this->plays) {
    char buf[MAX_STRING_CHARS];
    Str_Copy(buf, cgi.ConfigString(CS_TEAM_STATE), sizeof(buf));

    char *save = NULL;
    const char *entry = Str_Tokenize(buf, "\\", &save);
    for (int32_t i = 1; entry && i < this->team; i++) {
      entry = Str_Tokenize(NULL, "\\", &save);
    }

    if (entry) {
      Str_Copy(this->state, entry, sizeof(this->state));
    }

    const CGameTeamInfo *team = &cgameState.teams[this->team - 1];

    const char *caption = $(this, captionForTeam, team);
    $(counter->caption, setText, caption);

    const Color32 rgba = Color_Color32(team->color);
    const SDL_Color color = { rgba.r, rgba.g, rgba.b, 255 };

    if (memcmp(&color, &this->color, sizeof(color))) {
      this->color = color;

      $(counter->value->view.style, addColorAttribute, "color", &color);
      $((View *) counter->value, invalidateStyle);
    }
  } else {
    $(counter->caption, setText, " ");
  }

  super(View, self, updateBindings, data);
}

#pragma mark - CounterView

/**
 * @see CounterView::textForFrame(CounterView *, const ClientFrame *)
 * @details Every player sees the team scores, spectators too.
 */
static const char *textForFrame(CounterView *self, const ClientFrame *frame) {

  const TeamScoreView *this = (TeamScoreView *) self;

  if (!this->plays || this->state[0] == '\0') {
    return " ";
  }

  snprintf(self->text, sizeof(self->text), "%d", $(self, valueForFrame, frame));

  return self->text;
}

/**
 * @see CounterView::valueForFrame(CounterView *, const ClientFrame *)
 */
static int32_t valueForFrame(CounterView *self, const ClientFrame *frame) {
  return (int32_t) strtol(((TeamScoreView *) self)->state, NULL, 10);
}

#pragma mark - TeamScoreView

/**
 * @fn const char *TeamScoreView::captionForTeam(const TeamScoreView *self, const CGameTeamInfo *team)
 * @memberof TeamScoreView
 */
static const char *captionForTeam(const TeamScoreView *self, const CGameTeamInfo *team) {
  return team->name;
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->awakeWithDictionary = awakeWithDictionary;
  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

  ((CounterViewInterface *) clazz->interface)->textForFrame = textForFrame;
  ((CounterViewInterface *) clazz->interface)->valueForFrame = valueForFrame;

  ((TeamScoreViewInterface *) clazz->interface)->captionForTeam = captionForTeam;
}

/**
 * @fn Class *TeamScoreView::_TeamScoreView(void)
 * @memberof TeamScoreView
 */
Class *_TeamScoreView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "TeamScoreView",
      .superclass = _CounterView(),
      .instanceSize = sizeof(TeamScoreView),
      .interfaceSize = sizeof(TeamScoreViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
