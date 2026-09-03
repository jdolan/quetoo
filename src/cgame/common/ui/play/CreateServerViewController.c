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

#include "CreateServerViewController.h"
#include "MapListCollectionItemView.h"

#define _Class _CreateServerViewController

#pragma mark - Delegates

/**
 * @brief TextViewDelegate for the Bots field.
 * Stores (entered value + 1) into sv_min_clients, accounting for the
 * player themselves occupying one client slot.
 */
static void botsDidEndEditing(TextView *textView) {

  const String *string = (String *) textView->attributedText;
  cgi.SetCvarInteger("sv_min_clients", atoi(string->chars) + 1);
}

/**
 * @brief ButtonDelegate for the Create button.
 */
static void createServer(Button *button) {

  CreateServerViewController *this = button->delegate.self;

  PointerArray *selectedMaps = $(this->mapList, selectedMaps);
  if (selectedMaps->count) {

    file_t *file = cgi.OpenFileWrite(MAP_LIST_UI);
    if (file) {

      String *string = str("");
      assert(string);

      for (size_t i = 0; i < selectedMaps->count; i++) {
        const MapListItemInfo *info = (MapListItemInfo *) $(selectedMaps, get, i);

        char name[MAX_QPATH];
        StripExtension(Basename(info->mapname), name);

        $(string, appendFormat, "{\n\tname %s\n}\n", name);
      }

      const int64_t len = cgi.WriteFile(file, string->chars, string->length, 1);

      if (len == -1) {
        Cg_Warn("Failed to write %s\n", MAP_LIST_UI);
      } else {
        Cg_Debug("Wrote %s %"PRId64" bytes\n", MAP_LIST_UI, len);
      }

      release(string);

      cgi.CloseFile(file);

      cgi.SetCvarString("sv_map_list", MAP_LIST_UI);
      cgi.Cbuf("next_map");
    } else {
      Cg_Warn("Failed to create %s\n", MAP_LIST_UI);
    }
  } else {
    cgi.Print("No maps selected\n");
  }

  release(selectedMaps);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  CreateServerViewController *this = (CreateServerViewController *) self;

  View *gameplayInput, *hookInput, *techsInput;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("bots", &this->bots),
    MakeOutlet("gameplay", &this->gameplay),
    MakeOutlet("gameplayInput", &gameplayInput),
    MakeOutlet("movement", &this->movement),
    MakeOutlet("hookInput", &hookInput),
    MakeOutlet("techsInput", &techsInput),
    MakeOutlet("mapList", &this->mapList),
    MakeOutlet("create", &this->create)
  );

  $(self->view, awakeWithResourceName, "ui/play/CreateServerViewController.json");
  $(self->view, resolve, outlets);

#if !defined(G_HOOK)
  $(hookInput, removeFromSuperview);
#endif
  
#if !defined(G_TECH)
  $(techsInput, removeFromSuperview);
#endif

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/play/CreateServerViewController.css");
  assert(self->view->stylesheet);

  const cvar_t *sv_min_clients = cgi.GetCvar("sv_min_clients");
  const int32_t bots = sv_min_clients ? Maxi(0, sv_min_clients->integer - 1) : 0;
  $(this->bots, setDefaultText, va("%d", bots));

  this->bots->delegate.didEndEditing = botsDidEndEditing;

  $(this->gameplay, addOption, "Default", "default");

  size_t num_modes;
  const g_gameplay_t *modes = Cg_ListGameplayModes(&num_modes);
  if (num_modes <= 1) {
    $(gameplayInput, removeFromSuperview);
  } else {
    for (size_t i = 0; i < num_modes; i++) {
      $(this->gameplay, addOption, modes[i].label, (ident) modes[i].name);
    }
  }

  // "Default" defers to the level, as it does for gameplay
  $(this->movement, addOption, "Default", "default");

  for (size_t i = 0; i < Pm_MovementCount(); i++) {
    const pm_movement_info_t *movement = Pm_Movement((pm_movement_t) i);
    $(this->movement, addOption, movement->label, (ident) movement->name);
  }

  this->create->delegate.didClick = createServer;
  this->create->delegate.self = this;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

/**
 * @fn Class *CreateServerViewController::_CreateServerViewController(void)
 * @memberof CreateServerViewController
 */
Class *_CreateServerViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "CreateServerViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(CreateServerViewController),
      .interfaceSize = sizeof(CreateServerViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
