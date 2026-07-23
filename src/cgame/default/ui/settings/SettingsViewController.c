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

#include "SettingsViewController.h"

#define _Class _SettingsViewController

#pragma mark - Quality presets

typedef struct {
  int32_t shadows;
  int32_t shadowTileSize;
  int32_t lightingDistance;
  int32_t parallax;
  int32_t parallaxShadow;
  int32_t caustics;
  int32_t addWeather;
  int32_t addAtmospheric;
} QualityPreset;

static const QualityPreset qualityPresets[] = {
  [0] = { // Low
    .shadows                = 0,
    .shadowTileSize         = 128,
    .lightingDistance       = 1024,
    .parallax               = 0,
    .parallaxShadow         = 0,
    .caustics               = 0,
    .addWeather             = 0,
    .addAtmospheric         = 0,
  },
  [1] = { // Medium
    .shadows                = 1,
    .shadowTileSize         = 128,
    .lightingDistance       = 2048,
    .parallax               = 0,
    .parallaxShadow         = 0,
    .caustics               = 0,
    .addWeather             = 1,
    .addAtmospheric         = 1,
  },
  [2] = { // High
    .shadows                = 1,
    .shadowTileSize         = 256,
    .lightingDistance       = 4096,
    .parallax               = 1,
    .parallaxShadow         = 0,
    .caustics               = 1,
    .addWeather             = 1,
    .addAtmospheric         = 1,
  },
  [3] = { // Highest
    .shadows                = 1,
    .shadowTileSize         = 512,
    .lightingDistance       = 8192,
    .parallax               = 1,
    .parallaxShadow         = 1,
    .caustics               = 1,
    .addWeather             = 1,
    .addAtmospheric         = 1,
  },
};

/**
 * @brief Applies a graphics quality preset by setting all related renderer and game cvars.
 */
static void applyQualityPreset(const QualityPreset *p) {
  cgi.SetCvarInteger("r_shadows",           p->shadows);
  cgi.SetCvarInteger("r_shadow_tile_size",  p->shadowTileSize);
  cgi.SetCvarInteger("r_lighting_distance", p->lightingDistance);
  cgi.SetCvarInteger("r_parallax",          p->parallax);
  cgi.SetCvarInteger("r_parallax_shadow",   p->parallaxShadow);
  cgi.SetCvarInteger("r_caustics",          p->caustics);
  cgi.SetCvarInteger("cg_add_weather",      p->addWeather);
  cgi.SetCvarInteger("cg_add_atmospheric",  p->addAtmospheric);
}

/**
 * @return The index of the matching preset, or -1 if no preset matches.
 */
static intptr_t detectQualityPreset(void) {
  const QualityPreset current = {
    .shadows          = cgi.GetCvarInteger("r_shadows"),
    .shadowTileSize   = cgi.GetCvarInteger("r_shadow_tile_size"),
    .lightingDistance = cgi.GetCvarInteger("r_lighting_distance"),
    .parallax         = cgi.GetCvarInteger("r_parallax"),
    .parallaxShadow   = cgi.GetCvarInteger("r_parallax_shadow"),
    .caustics         = cgi.GetCvarInteger("r_caustics"),
    .addWeather       = cgi.GetCvarInteger("cg_add_weather"),
    .addAtmospheric   = cgi.GetCvarInteger("cg_add_atmospheric"),
  };

  for (size_t i = 0; i < lengthof(qualityPresets); i++) {
    if (memcmp(&current, &qualityPresets[i], sizeof(QualityPreset)) == 0) {
      return (intptr_t) i;
    }
  }

  return -1;
}

#pragma mark - Delegates

/**
 * @brief ButtonDelegate for the Apply button.
 */
static void didClickApply(Button *button) {
  cgi.Cbuf("r_restart; s_restart");
}

/**
 * @brief SelectDelegate callback for quality presets.
 */
static void didSelectQuality(Select *select, Option *option) {

  const intptr_t index = (intptr_t) option->value;
  if (index >= 0 && index < (intptr_t) lengthof(qualityPresets)) {
    applyQualityPreset(&qualityPresets[index]);
  }

  ViewController *this = select->delegate.self;
  if (this) {
    $(this->view, updateBindings);
  }
}

/**
 * @brief SelectDelegate for the resolution Select.
 */
static void didSelectResolution(Select *select, Option *option) {

  const intptr_t value = (intptr_t) option->value;

  const int32_t w  = (value >> 16) & 0xFFFF;
  const int32_t h =  (value >>  0) & 0xFFFF;

  cgi.SetCvarInteger("r_fullscreen_width", w);
  cgi.SetCvarInteger("r_fullscreen_height", h);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  View *view = $$(View, viewWithResourceName, "ui/settings/SettingsViewController.json", NULL);
  assert(view);

  $(self, setView, view);
  release(view);

  Select *windowMode, *resolution, *verticalSync, *anisotropy, *antialias, *quality;
  View *rtx, *rtxOverlay;
  Button *apply;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("windowMode", &windowMode),
    MakeOutlet("resolution", &resolution),
    MakeOutlet("verticalSync", &verticalSync),
    MakeOutlet("anisotropy", &anisotropy),
    MakeOutlet("antialias", &antialias),
    MakeOutlet("quality", &quality),
    MakeOutlet("rtx", &rtx),
    MakeOutlet("rtxOverlay", &rtxOverlay),
    MakeOutlet("apply", &apply)
  );

  $(self->view, resolve, outlets);

  if (!cgi.GetCvarInteger("r_rtx_supported")) {
    rtx->hidden = true;
    rtxOverlay->hidden = true;
  }

  $(windowMode, addOption, "Window", (ident) 0);
  $(windowMode, addOption, "Fullscreen", (ident) 1);
  $(windowMode, addOption, "Exclusive Fullscreen", (ident) 2);

  $(resolution, addOption, "Desktop", (ident) 0);

  int32_t num_modes;
  SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(cgi.context->display, &num_modes);
  if (modes) {
    int32_t last_w = 0, last_h = 0;
    for (int32_t i = 0; i < num_modes; i++) {

      const SDL_DisplayMode *mode = modes[i];
      if (mode->pixel_density > 1.f) {
        continue;
      }

      const int32_t w = mode->w, h = mode->h;
      if (w == last_w && h == last_h) {
        continue;
      }

      last_w = w;
      last_h = h;

      char label[MAX_QPATH];
      q_snprintf(label, sizeof(label), "%dx%d", w, h);
      $(resolution, addOption, label, (ident) (intptr_t) ((w << 16) | h));
    }
    SDL_free(modes);
  }

  const int32_t w = cgi.GetCvarInteger("r_fullscreen_width");
  const int32_t h = cgi.GetCvarInteger("r_fullscreen_height");

  $(resolution, selectOptionWithValue, (ident) (intptr_t) ((w << 16) | h));

  resolution->delegate.self = self;
  resolution->delegate.didSelectOption = didSelectResolution;

  $(verticalSync, addOption, "Disabled", (ident) 0);
  $(verticalSync, addOption, "Enabled", (ident) 1);
  $(verticalSync, addOption, "Adaptive", (ident) -1);

  $(anisotropy, addOption, "Disabled", (ident) 0);
  $(anisotropy, addOption, "2x", (ident) 2);
  $(anisotropy, addOption, "4x", (ident) 4);
  $(anisotropy, addOption, "8x", (ident) 8);
  $(anisotropy, addOption, "16x", (ident) 16);

  $(antialias, addOption, "Disabled", (ident) 0);
  $(antialias, addOption, "2x", (ident) 2);
  $(antialias, addOption, "4x", (ident) 4);
  $(antialias, addOption, "8x", (ident) 8);

  $(quality, addOption, "Custom", (ident) -1);
  $(quality, addOption, "Low", (ident) 0);
  $(quality, addOption, "Medium", (ident) 1);
  $(quality, addOption, "High", (ident) 2);
  $(quality, addOption, "Highest", (ident) 3);

  quality->delegate.self = self;
  quality->delegate.didSelectOption = didSelectQuality;

  $(quality, selectOptionWithValue, (ident) detectQualityPreset());

  apply->delegate.didClick = didClickApply;
  apply->delegate.self = self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {
  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

/**
 * @fn Class *SettingsViewController::_SettingsViewController(void)
 * @memberof SettingsViewController
 */
Class *_SettingsViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "SettingsViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(SettingsViewController),
      .interfaceOffset = offsetof(SettingsViewController, interface),
      .interfaceSize = sizeof(SettingsViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
