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
#include "bg_vote.h"

#include "VoteViewController.h"

#define _Class _VoteViewController

#pragma mark - Delegates

static void didClickYes(Button *button) {
  Cg_Vote_Cast(true);
  cgi.SetKeyDest(KEY_GAME);
}

static void didClickNo(Button *button) {
  Cg_Vote_Cast(false);
  cgi.SetKeyDest(KEY_GAME);
}

/**
 * @brief Shows the argument control the selected vote type takes.
 */
static void didSelectType(Select *select, Option *option) {

  VoteViewController *this = select->delegate.self;
  const vote_type_t *type = option->value;

  $(((View *) this->map)->superview, setHidden, type->arg != VOTE_ARG_MAP);
  $(((View *) this->client)->superview, setHidden, type->arg != VOTE_ARG_CLIENT);
  $(((View *) this->value)->superview, setHidden, type->arg != VOTE_ARG_INTEGER);

  if (type->arg == VOTE_ARG_INTEGER) {
    this->value->min = type->min;
    this->value->max = type->max;
    this->value->step = 1.0;
    $(this->value, setValue, Clampf(this->value->value, type->min, type->max));
  }
}

static void didClickCall(Button *button) {

  VoteViewController *this = button->delegate.self;

  const Option *selected = $(this->type, selectedOption);
  if (!selected) {
    return;
  }

  const vote_type_t *type = selected->value;
  const char *arg = "";

  switch (type->arg) {
    case VOTE_ARG_NONE:
      break;
    case VOTE_ARG_MAP: {
      const Option *option = $(this->map, selectedOption);
      if (!option) {
        return;
      }
      arg = option->title->text;
      break;
    }
    case VOTE_ARG_CLIENT: {
      const Option *option = $(this->client, selectedOption);
      if (!option) {
        return;
      }
      arg = option->title->text;
      break;
    }
    case VOTE_ARG_INTEGER:
      arg = va("%d", (int32_t) this->value->value);
      break;
  }

  Cg_Vote_Call(type->name, arg);
  cgi.SetKeyDest(KEY_GAME);
}

#pragma mark - Options

/**
 * @brief Fs_Enumerator adding each installed map to the map select.
 */
static void enumerateMaps(const char *path, void *data) {

  Select *select = data;

  char name[MAX_QPATH];
  StripExtension(Basename(path), name);

  $(select, addOption, name, NULL);
}

static void refreshClients(VoteViewController *this) {

  $(this->client, removeAllOptions);

  for (int32_t i = 0; i < cg_state.max_clients; i++) {
    const cg_client_info_t *ci = &cg_state.clients[i];
    if (*ci->name) {
      $(this->client, addOption, ci->name, NULL);
    }
  }
}

static void refreshStatus(VoteViewController *this) {

  const bool active = cg_state.vote.active;

  if (active) {
    $(this->status->text, setText, va("%s called a vote: %s%s%s  (Yes %d  No %d of %d)",
                                      cg_state.vote.initiator, cg_state.vote.type,
                                      *cg_state.vote.arg ? " " : "", cg_state.vote.arg,
                                      cg_state.vote.yes, cg_state.vote.no, cg_state.vote.eligible));
  } else {
    $(this->status->text, setText, "No vote is in progress");
  }

  $((View *) this->yes, setHidden, !active);
  $((View *) this->no, setHidden, !active);
}

#pragma mark - ViewController

static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  VoteViewController *this = (VoteViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("status", &this->status),
    MakeOutlet("yes", &this->yes),
    MakeOutlet("no", &this->no),
    MakeOutlet("type", &this->type),
    MakeOutlet("map", &this->map),
    MakeOutlet("client", &this->client),
    MakeOutlet("value", &this->value),
    MakeOutlet("call", &this->call)
  );

  View *view = $$(View, viewWithResourceName, "ui/vote/VoteViewController.json", outlets);
  assert(view);

  this->yes->delegate.didClick = didClickYes;
  this->yes->delegate.self = self;

  this->no->delegate.didClick = didClickNo;
  this->no->delegate.self = self;

  this->call->delegate.didClick = didClickCall;
  this->call->delegate.self = self;

  this->type->delegate.didSelectOption = didSelectType;
  this->type->delegate.self = self;

  size_t count;
  const vote_type_t *types = Cg_ListVoteTypes(&count);
  for (size_t i = 0; i < count; i++) {
    $(this->type, addOption, types[i].title, (ident) &types[i]);
  }

  $(this->map, addOption, "next", NULL);
  cgi.EnumerateFiles("maps/*.bsp", enumerateMaps, this->map);

  $(this->value, setLabelFormat, "%.0f");

  $(self, setView, view);
  release(view);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/vote/VoteViewController.css");
  assert(self->view->stylesheet);
}

static void viewWillAppear(ViewController *self) {

  super(ViewController, self, viewWillAppear);

  VoteViewController *this = (VoteViewController *) self;

  refreshStatus(this);
  refreshClients(this);

  const Option *selected = $(this->type, selectedOption);
  if (selected) {
    didSelectType(this->type, (Option *) selected);
  }
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {
  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;
}

Class *_VoteViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "VoteViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(VoteViewController),
      .interfaceSize = sizeof(VoteViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
