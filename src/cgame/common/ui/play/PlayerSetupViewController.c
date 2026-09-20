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

#include <Objectively/Regexp.h>

#include "PlayerSetupViewController.h"

#define _Class _PlayerSetupViewController

#pragma mark - Delegates

/**
 * @brief Comparator for the model and skin Selects.
 */
static Order sortOptions(const ident a, const ident b) {

  const char *c = ((const Option *) a)->title->text;
  const char *d = ((const Option *) b)->title->text;

  return q_strcmp(c, d) < 0 ? OrderAscending : OrderDescending;
}

/**
 * @brief Fs_Enumerator counting the matched files, for existence checks.
 */
static void countFiles(const char *path, void *data) {
  int32_t *count = (int32_t *) data;
  (*count)++;
}

/**
 * @brief Selects the Option in `select` whose title matches `title`, if any.
 * @return True if a matching Option was found and selected.
 */
static bool selectOptionWithTitle(Select *select, const char *title) {

  const Array *options = (Array *) select->options;
  for (size_t i = 0; i < options->count; i++) {

    Option *option = (Option *) $(options, objectAtIndex, i);
    if (q_strcmp(option->title->text, title) == 0) {
      $(select, selectOption, option);
      return true;
    }
  }

  return false;
}

/**
 * @brief Fs_Enumerator for resolving the skin variants of the currently
 * selected model into skinSelect.
 */
static void enumerateSkins(const char *path, void *data) {

  PlayerSetupViewController *this = (PlayerSetupViewController *) data;

  // model and skin names may contain lowercase letters, digits, underscores
  // and hyphens (e.g. "q4hybrid-low/lo_timex_blue_blue")
  Regexp *regexp = re("players/[a-z0-9_-]+/([a-z0-9_-]+)\\.skin", 0);

  Range *matches;
  if ($(regexp, matchesCharacters, path, 0, &matches)) {

    const Range *skin = &matches[1];

    char title[MAX_QPATH];
    q_snprintf(title, sizeof(title), "%.*s", (int) skin->length, path + skin->location);

    const Array *options = (Array *) this->skinSelect->options;
    bool exists = false;
    for (size_t i = 0; i < options->count; i++) {
      const Option *option = $(options, objectAtIndex, i);
      if (q_strcmp(option->title->text, title) == 0) {
        exists = true;
        break;
      }
    }

    if (!exists) {
      $(this->skinSelect, addOption, title, NULL);
    }

    free(matches);
  }

  release(regexp);
}

/**
 * @brief Repopulates skinSelect for the given model, selecting the variant
 * named by cg_skin if it names this model, else "default", else the first
 * available skin.
 */
static void refreshSkins(PlayerSetupViewController *this, const char *model) {

  $(this->skinSelect, removeAllOptions);

  cgi.EnumerateFiles(va("players/%s/*.skin", model), enumerateSkins, this);

  const Array *options = (Array *) this->skinSelect->options;
  if (options->count == 0) {
    return;
  }

  char prefix[MAX_QPATH];
  q_snprintf(prefix, sizeof(prefix), "%s/", model);

  bool selected = false;
  if (!strncmp(cg_skin->string, prefix, strlen(prefix))) {
    selected = selectOptionWithTitle(this->skinSelect, cg_skin->string + strlen(prefix));
  }

  if (!selected) {
    selected = selectOptionWithTitle(this->skinSelect, "default");
  }

  if (!selected) {
    Option *first = (Option *) $(options, objectAtIndex, 0);
    $(this->skinSelect, selectOption, first);
  }
}

/**
 * @brief Fs_Enumerator for resolving the available player models into
 * modelSelect. Directories with no `.skin` files (e.g. `players/common`,
 * which holds shared sounds, not a model) are skipped.
 */
static void enumerateModels(const char *path, void *data) {

  PlayerSetupViewController *this = (PlayerSetupViewController *) data;

  // model names may contain lowercase letters, digits, underscores and
  // hyphens (e.g. "q4hybrid-low")
  Regexp *regexp = re("players/([a-z0-9_-]+)$", 0);

  Range *matches;
  if ($(regexp, matchesCharacters, path, 0, &matches)) {

    const Range *model = &matches[1];

    char title[MAX_QPATH];
    q_snprintf(title, sizeof(title), "%.*s", (int) model->length, path + model->location);

    int32_t count = 0;
    cgi.EnumerateFiles(va("%s/*.skin", path), countFiles, &count);

    if (count > 0) {
      const Array *options = (Array *) this->modelSelect->options;
      bool exists = false;
      for (size_t i = 0; i < options->count; i++) {
        const Option *option = $(options, objectAtIndex, i);
        if (q_strcmp(option->title->text, title) == 0) {
          exists = true;
          break;
        }
      }

      if (!exists) {
        $(this->modelSelect, addOption, title, NULL);
      }
    }

    free(matches);
  }

  release(regexp);
}

/**
 * @brief ActionFunction for model selection.
 */
static void didSelectModel(Select *select, Option *option) {

  PlayerSetupViewController *this = (PlayerSetupViewController *) select->delegate.self;

  refreshSkins(this, option->title->text);

  const Option *skin = $(this->skinSelect, selectedOption);
  if (skin) {
    cgi.SetCvarString(cg_skin->name, va("%s/%s", option->title->text, skin->title->text));
  }

  $((View *) this->playerModelView, updateBindings, NULL);
}

/**
 * @brief ActionFunction for skin selection.
 */
static void didSelectSkin(Select *select, Option *option) {

  PlayerSetupViewController *this = (PlayerSetupViewController *) select->delegate.self;

  const Option *model = $(this->modelSelect, selectedOption);
  if (model) {
    cgi.SetCvarString(cg_skin->name, va("%s/%s", model->title->text, option->title->text));

    $((View *) this->playerModelView, updateBindings, NULL);
  }
}

/**
 * @brief HueColorPickerDelegate callback for effect color selection.
 */
static void didPickEffectColor(HueColorPicker *hueColorPicker, double hue, double saturation, double value) {

  PlayerSetupViewController *this = hueColorPicker->delegate.self;

  if (hue < 0.0) {
    cgi.SetCvarString(cg_color->name, "default");

    $(this->effectsColorPicker->colorView->style, addColorAttribute, "background-color", &Colors.Charcoal);
    $(this->effectsColorPicker->colorView, invalidateStyle);

    $(this->effectsColorPicker->hueSlider->label, setText, "");
  } else {
    cgi.SetCvarInteger(cg_color->name, (int32_t) hueColorPicker->hue);
  }
}

/**
 * @brief HSVColorPickerDelegate callback for player color selection.
 */
static void didPickPlayerColor(HSVColorPicker *hsvColorPicker, double hue, double saturation, double value) {

  PlayerSetupViewController *this = hsvColorPicker->delegate.self;

  Cvar *var = NULL;
  if (hsvColorPicker == this->helmetColorPicker) {
    var = cg_helmet;
  } else if (hsvColorPicker == this->pantsColorPicker) {
    var = cg_pants;
  } else if (hsvColorPicker == this->shirtColorPicker) {
    var = cg_shirt;
  }
  assert(var);

  if (hue < 0.0) {
    cgi.SetCvarString(var->name, "default");

    $(hsvColorPicker->colorView->style, addColorAttribute, "background-color", &Colors.Charcoal);
    $(hsvColorPicker->colorView, invalidateStyle);

    $(hsvColorPicker->hueSlider->label, setText, "");
  } else {
    const SDL_Color color = $(hsvColorPicker, rgbColor);
    cgi.SetCvarString(var->name, MVC_RGBToHex(&color));
  }

  if (this->playerModelView) {
    $((View *) this->playerModelView, updateBindings, NULL);
  }
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  PlayerSetupViewController *this = (PlayerSetupViewController *) self;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("name", &this->name),
    MakeOutlet("model", &this->modelSelect),
    MakeOutlet("skin", &this->skinSelect),
    MakeOutlet("helmet", &this->helmetColorPicker),
    MakeOutlet("shirt", &this->shirtColorPicker),
    MakeOutlet("pants", &this->pantsColorPicker),
    MakeOutlet("effects", &this->effectsColorPicker),
    MakeOutlet("hand", &this->hand),
    MakeOutlet("player", &this->playerModelView)
  );

  $(self->view, awakeWithResourceName, "ui/play/PlayerSetupViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/play/PlayerSetupViewController.css");
  assert(self->view->stylesheet);

  this->modelSelect->comparator = sortOptions;
  this->modelSelect->delegate.self = this;
  this->modelSelect->delegate.didSelectOption = didSelectModel;

  this->skinSelect->comparator = sortOptions;
  this->skinSelect->delegate.self = this;
  this->skinSelect->delegate.didSelectOption = didSelectSkin;

  cgi.EnumerateFiles("players/*", enumerateModels, this);

  char model[MAX_QPATH];
  const char *slash = strchr(cg_skin->string, '/');
  if (slash) {
    q_snprintf(model, sizeof(model), "%.*s", (int) (slash - cg_skin->string), cg_skin->string);
  } else {
    model[0] = '\0';
  }

  if (!*model || !selectOptionWithTitle(this->modelSelect, model)) {
    const Array *options = (Array *) this->modelSelect->options;
    if (options->count) {
      Option *first = (Option *) $(options, objectAtIndex, 0);
      $(this->modelSelect, selectOption, first);
    }
  }

  const Option *selectedModel = $(this->modelSelect, selectedOption);
  if (selectedModel) {
    refreshSkins(this, selectedModel->title->text);
  }

  this->effectsColorPicker->delegate.self = this;
  this->effectsColorPicker->delegate.didPickColor = didPickEffectColor;

  this->helmetColorPicker->delegate.self = self;
  this->helmetColorPicker->delegate.didPickColor = didPickPlayerColor;

  this->shirtColorPicker->delegate.self = self;
  this->shirtColorPicker->delegate.didPickColor = didPickPlayerColor;

  this->pantsColorPicker->delegate.self = self;
  this->pantsColorPicker->delegate.didPickColor = didPickPlayerColor;

  $(this->hand, addOption, "Center", (ident) 0);
  $(this->hand, addOption, "Right", (ident) 1);
  $(this->hand, addOption, "Left", (ident) 2);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  super(ViewController, self, viewWillAppear);

  PlayerSetupViewController *this = (PlayerSetupViewController *) self;

  if (q_strcmp(cg_color->string, "default")) {
    $(this->effectsColorPicker, setColor, cg_color->integer, 1.0, 1.0);
  } else {
    $(this->effectsColorPicker, setColor, -1.0, 1.0, 1.0);
    didPickEffectColor(this->effectsColorPicker, -1.0, 1.0, 1.0);
  }

  const SDL_Color helmet = MVC_HexToRGBA(cg_helmet->string);
  if (helmet.r || helmet.g || helmet.b) {
    $(this->helmetColorPicker, setRGBColor, &helmet);
  } else {
    $(this->helmetColorPicker, setColor, -1.0, 1.0, 1.0);
    didPickPlayerColor(this->helmetColorPicker, -1.0, 1.0, 1.0);
  }

  const SDL_Color shirt = MVC_HexToRGBA(cg_shirt->string);
  if (shirt.r || shirt.g || shirt.b) {
    $(this->shirtColorPicker, setRGBColor, &shirt);
  } else {
    $(this->shirtColorPicker, setColor, -1.0, 1.0, 1.0);
    didPickPlayerColor(this->shirtColorPicker, -1.0, 1.0, 1.0);
  }

  const SDL_Color pants = MVC_HexToRGBA(cg_pants->string);
  if (pants.r || pants.g || pants.b) {
    $(this->pantsColorPicker, setRGBColor, &pants);
  } else {
    $(this->pantsColorPicker, setColor, -1.0, 1.0, 1.0);
    didPickPlayerColor(this->pantsColorPicker, -1.0, 1.0, 1.0);
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
 * @fn Class *PlayerSetupViewController::_PlayerSetupViewController(void)
 * @memberof PlayerSetupViewController
 */
Class *_PlayerSetupViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "PlayerSetupViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(PlayerSetupViewController),
      .interfaceSize = sizeof(PlayerSetupViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
