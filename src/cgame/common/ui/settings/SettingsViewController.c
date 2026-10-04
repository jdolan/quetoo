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

#include "CvarSelect.h"
#include "SettingsViewController.h"

#define _Class _SettingsViewController

#pragma mark - Quality presets

static const struct {
  const char *identifier;
  const char *enabled;
  const char *quality;
  bool intensity;
} effectSettings[] = {
  { "shadowQuality", "r_shadows", "r_shadowQuality", false },
  { "parallaxQuality", "r_parallax", "r_parallaxQuality", true },
  { "parallaxShadowQuality", "r_parallaxShadow", "r_parallaxShadowQuality", false },
  { "portalsQuality", "r_portals", "r_portalsQuality", false },
  { "reflectionsQuality", "r_reflections", "r_reflectionsQuality", false },
};

typedef struct {
  int32_t shadows;
  int32_t shadowTileSize;
  int32_t lightingDistance;
  int32_t parallax;
  int32_t parallaxShadow;
  int32_t caustics;
  int32_t addWeather;
  int32_t addAtmospheric;
  int32_t reflections;
  int32_t portals;
  int32_t effectQuality[5];
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
    .reflections            = 0,
    .portals                = 0,
    .effectQuality          = { 1, 1, 1, 1, 1 },
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
    .reflections            = 0,
    .portals                = 1,
    .effectQuality          = { 1, 1, 1, 1, 1 },
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
    .reflections            = 1,
    .portals                = 1,
    .effectQuality          = { 2, 2, 2, 2, 2 },
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
    .reflections            = 1,
    .portals                = 1,
    .effectQuality          = { 3, 3, 3, 3, 3 },
  },
};

/**
 * @brief Applies a graphics quality preset by setting all related renderer and game cvars.
 */
static void applyQualityPreset(const QualityPreset *p) {
  cgi.SetCvarInteger("r_shadows",           p->shadows);
  cgi.SetCvarInteger("r_shadowTileSize",    p->shadowTileSize);
  cgi.SetCvarInteger("r_lightingDistance",  p->lightingDistance);
  cgi.SetCvarInteger("r_parallax",          p->parallax);
  cgi.SetCvarInteger("r_parallaxShadow",    p->parallaxShadow);
  cgi.SetCvarInteger("r_caustics",          p->caustics);
  cgi.SetCvarInteger("cg_addWeather",       p->addWeather);
  cgi.SetCvarInteger("cg_addAtmospheric",   p->addAtmospheric);
  cgi.SetCvarInteger("r_reflections",       p->reflections);
  cgi.SetCvarInteger("r_portals",           p->portals);
  for (size_t i = 0; i < lengthof(effectSettings); i++) {
    cgi.SetCvarInteger(effectSettings[i].quality, p->effectQuality[i]);
  }
}

/**
 * @return The index of the matching preset, or -1 if no preset matches.
 */
static intptr_t detectQualityPreset(void) {
  QualityPreset current = {
    .shadows          = cgi.GetCvarInteger("r_shadows"),
    .shadowTileSize   = cgi.GetCvarInteger("r_shadowTileSize"),
    .lightingDistance = cgi.GetCvarInteger("r_lightingDistance"),
    .parallax         = cgi.GetCvarInteger("r_parallax"),
    .parallaxShadow   = cgi.GetCvarInteger("r_parallaxShadow"),
    .caustics         = cgi.GetCvarInteger("r_caustics"),
    .addWeather       = cgi.GetCvarInteger("cg_addWeather"),
    .addAtmospheric   = cgi.GetCvarInteger("cg_addAtmospheric"),
    .reflections      = cgi.GetCvarInteger("r_reflections"),
    .portals          = cgi.GetCvarInteger("r_portals"),
  };

  if (cgi.GetCvarValue("r_parallax") != current.parallax ||
      cgi.GetCvarValue("r_caustics") != current.caustics) {
    return -1;
  }

  for (size_t i = 0; i < lengthof(effectSettings); i++) {
    current.effectQuality[i] = cgi.GetCvarInteger(effectSettings[i].quality);
    if (cgi.GetCvarValue(effectSettings[i].quality) != current.effectQuality[i]) {
      return -1;
    }
  }

  for (size_t i = 0; i < lengthof(qualityPresets); i++) {
    if (memcmp(&current, &qualityPresets[i], sizeof(QualityPreset)) == 0) {
      return (intptr_t) i;
    }
  }

  return -1;
}

#pragma mark - Frame limiter

/**
 * @brief The frame limiter options, and the values of cl_maxFps they set.
 */
static const struct {
  const char *title;
  int32_t value;
} maxFpsOptions[] = {
  { "Unlimited", -1 },
  { "Refresh rate", 0 },
  { "60", 60 },
  { "120", 120 },
  { "144", 144 },
  { "165", 165 },
  { "240", 240 },
};

/**
 * @brief The frame limiter Select value for a cl_maxFps that matches no option.
 */
#define MAX_FPS_CUSTOM -2

/**
 * @return The matching cl_maxFps option value, or MAX_FPS_CUSTOM.
 */
static intptr_t detectMaxFps(void) {

  const float maxFps = cgi.GetCvarValue("cl_maxFps");

  for (size_t i = 0; i < lengthof(maxFpsOptions); i++) {
    if (maxFps == maxFpsOptions[i].value) {
      return maxFpsOptions[i].value;
    }
  }

  return MAX_FPS_CUSTOM;
}

#pragma mark - Delegates

static void refreshQualityControls(SettingsViewController *self) {

  for (size_t i = 0; i < lengthof(effectSettings); i++) {
    const Cvar *enabled = cgi.GetCvar(effectSettings[i].enabled);
    const bool active = effectSettings[i].intensity ? enabled->value != 0.f : enabled->integer != 0;
    const int32_t quality = clamp(cgi.GetCvarInteger(effectSettings[i].quality), 1, 3);
    $(self->effects[i], setValue, active ? quality : 0);
  }

  $(self->shadowResolution, setValue, cgi.GetCvarInteger("r_shadowTileSize"));
  $(self->lightingDistance, setValue, cgi.GetCvarValue("r_lightingDistance"));
  $(self->bloom, setValue, cgi.GetCvarValue("r_bloom") > 0.f
    ? max(2, cgi.GetCvarInteger("r_bloomIterations")) : 0);
  $(self->quality, selectOptionWithValue, (ident) detectQualityPreset());
  $(self->viewController.view, updateBindings, NULL);
}

static void didSetEffectQuality(Slider *slider, double value) {

  SettingsViewController *this = slider->delegate.self;

  for (size_t i = 0; i < lengthof(effectSettings); i++) {
    if (this->effects[i] != slider) {
      continue;
    }

    const Cvar *enabled = cgi.GetCvar(effectSettings[i].enabled);
    if (value == 0) {
      cgi.SetCvarInteger(enabled->name, 0);
    } else {
      cgi.SetCvarInteger(effectSettings[i].quality, (int32_t) value);
      const bool active = effectSettings[i].intensity ? enabled->value != 0.f : enabled->integer != 0;
      if (!active) {
        cgi.SetCvarString(enabled->name, enabled->defaultString);
      }
    }

    refreshQualityControls(this);
    return;
  }

  assert(false);
}

static void didSetShadowResolution(Slider *slider, double value) {
  cgi.SetCvarInteger("r_shadowTileSize", (int32_t) value);
  refreshQualityControls(slider->delegate.self);
}

static void didSetLightingDistance(Slider *slider, double value) {
  cgi.SetCvarInteger("r_lightingDistance", (int32_t) value);
  refreshQualityControls(slider->delegate.self);
}

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

  SettingsViewController *this = select->delegate.self;
  if (this) {
    refreshQualityControls(this);
  }
}

/**
 * @brief SelectDelegate callback for the frame limiter.
 */
static void didSelectMaxFps(Select *select, Option *option) {

  const int32_t value = (int32_t) (intptr_t) option->value;
  if (value != MAX_FPS_CUSTOM) {
    cgi.SetCvarInteger("cl_maxFps", value);
  }
}

/**
 * @brief SliderDelegate callback for bloom.
 * @details The Slider chooses the blur iterations. A player who tuned the bloom intensity keeps it,
 * and only enabling bloom from Off restores the default intensity.
 */
static void didSetBloom(Slider *slider, double value) {

  if (value == 0) {
    cgi.SetCvarInteger("r_bloom", 0);
  } else {
    if (cgi.GetCvarValue("r_bloom") <= 0.f) {
      cgi.SetCvarString("r_bloom", cgi.GetCvar("r_bloom")->defaultString);
    }
    cgi.SetCvarInteger("r_bloomIterations", (int32_t) value);
  }

  refreshQualityControls(slider->delegate.self);
}

/**
 * @brief TabViewDelegate callback, so that each tab shows what the other changed.
 * @details A change on the Advanced tab can turn a preset into Custom.
 */
static void didSelectTab(TabView *tabView, TabViewItem *tab) {

  SettingsViewController *this = tabView->delegate.self;

  refreshQualityControls(this);
}

/**
 * @brief TextViewDelegate callback for binding keys.
 */
static void didBindKey(TextView *textView) {

  const ViewController *this = textView->delegate.self;

  $(this->view, updateBindings, NULL);
}

/**
 * @brief ViewEnumerator for setting the TextViewDelegate on BindTextViews.
 */
static void setBindDelegate(View *view, ident data) {

  ((TextView *) view)->delegate = (TextViewDelegate) {
    .self = data,
    .didEndEditing = didBindKey
  };
}

/**
 * @brief Adds a tab whose view is inflated from the given layout.
 */
static void addTab(TabViewController *tabViewController, const char *name) {

  ViewController *viewController = $(alloc(ViewController), init);
  assert(viewController);

  $(viewController, loadViewIfNeeded);
  $(viewController->view, awakeWithResourceName, name);

  $((ViewController *) tabViewController, addChildViewController, viewController);
  release(viewController);
}

/**
 * @brief Adds the system default and each of the given audio devices to the Select.
 * @remarks A configured device that is not connected is offered too, so that the Select shows the
 * player's choice rather than nothing.
 */
static void addAudioDevices(CvarSelect *select, SDL_AudioDeviceID *devices, int32_t count) {

  if (!select->var) {
    SDL_free(devices);
    return;
  }

  $((Select *) select, addOption, "System default", (ident) "");

  bool found = !select->var->string[0];

  for (int32_t i = 0; i < count; i++) {
    const char *name = SDL_GetAudioDeviceName(devices[i]);
    if (name) {
      $((Select *) select, addOption, name, NULL);
      found |= !Str_Compare(name, select->var->string);
    }
  }

  SDL_free(devices);

  if (!found) {
    $((Select *) select, addOption, select->var->string, NULL);
  }
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  SettingsViewController *this = (SettingsViewController *) self;

  release(this->tabViewController);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  SettingsViewController *this = (SettingsViewController *) self;

  View *view = $$(View, viewWithResourceName, "ui/settings/SettingsViewController.json", NULL);
  assert(view);

  $(self, setView, view);
  release(view);

  this->tabViewController = $(alloc(TabViewController), init);
  assert(this->tabViewController);

  addTab(this->tabViewController, "ui/settings/GraphicsSettings.json");
  addTab(this->tabViewController, "ui/settings/SoundSettings.json");
  addTab(this->tabViewController, "ui/settings/AdvancedSettings.json");

  $(self, addChildViewController, (ViewController *) this->tabViewController);
  $((View *) ((Panel *) view)->contentView, addSubview, ((ViewController *) this->tabViewController)->view);

  this->tabViewController->tabView->delegate.self = this;
  this->tabViewController->tabView->delegate.didSelectTab = didSelectTab;

  Select *windowMode, *maxFps, *verticalSync, *anisotropy, *antialias;
  CvarSelect *playbackDevice, *captureDevice;
  Button *apply;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("windowMode", &windowMode),
    MakeOutlet("maxFps", &maxFps),
    MakeOutlet("verticalSync", &verticalSync),
    MakeOutlet("anisotropy", &anisotropy),
    MakeOutlet("antialias", &antialias),
    MakeOutlet("quality", &this->quality),
    MakeOutlet("shadowResolution", &this->shadowResolution),
    MakeOutlet("lightingDistance", &this->lightingDistance),
    MakeOutlet("bloom", &this->bloom),
    MakeOutlet("playbackDevice", &playbackDevice),
    MakeOutlet("captureDevice", &captureDevice),
    MakeOutlet("apply", &apply)
  );

  $(self->view, resolve, outlets);

  static_assert(lengthof(effectSettings) == lengthof(this->effects), "Effect quality outlets");
  for (size_t i = 0; i < lengthof(effectSettings); i++) {
    Outlet effectOutlets[] = MakeOutlets(MakeOutlet(effectSettings[i].identifier, &this->effects[i]));
    $(self->view, resolve, effectOutlets);
    this->effects[i]->delegate = (SliderDelegate) {
      .self = this,
      .didSetValue = didSetEffectQuality,
    };
  }

  this->shadowResolution->delegate = (SliderDelegate) {
    .self = this,
    .didSetValue = didSetShadowResolution,
  };

  this->lightingDistance->delegate = (SliderDelegate) {
    .self = this,
    .didSetValue = didSetLightingDistance,
  };

  this->bloom->delegate = (SliderDelegate) {
    .self = this,
    .didSetValue = didSetBloom,
  };

  $(self->view, enumerateSelection, "BindTextView", setBindDelegate, self);

  $(windowMode, addOption, "Window", (ident) 0);
  $(windowMode, addOption, "Fullscreen", (ident) 1);
  $(windowMode, addOption, "Exclusive Fullscreen", (ident) 2);

  $(verticalSync, addOption, "Disabled", (ident) 0);
  $(verticalSync, addOption, "Enabled", (ident) 1);
  $(verticalSync, addOption, "Adaptive", (ident) -1);

  $(maxFps, addOption, "Custom", (ident) MAX_FPS_CUSTOM);
  for (size_t i = 0; i < lengthof(maxFpsOptions); i++) {
    $(maxFps, addOption, maxFpsOptions[i].title, (ident) (intptr_t) maxFpsOptions[i].value);
  }

  maxFps->delegate.self = self;
  maxFps->delegate.didSelectOption = didSelectMaxFps;

  $(maxFps, selectOptionWithValue, (ident) detectMaxFps());

  $(anisotropy, addOption, "Disabled", (ident) 0);
  $(anisotropy, addOption, "2x", (ident) 2);
  $(anisotropy, addOption, "4x", (ident) 4);
  $(anisotropy, addOption, "8x", (ident) 8);
  $(anisotropy, addOption, "16x", (ident) 16);

  $(antialias, addOption, "Disabled", (ident) 0);
  $(antialias, addOption, "2x", (ident) 2);
  $(antialias, addOption, "4x", (ident) 4);
  $(antialias, addOption, "8x", (ident) 8);

  $(this->quality, addOption, "Custom", (ident) -1);
  $(this->quality, addOption, "Low", (ident) 0);
  $(this->quality, addOption, "Medium", (ident) 1);
  $(this->quality, addOption, "High", (ident) 2);
  $(this->quality, addOption, "Highest", (ident) 3);

  this->quality->delegate.self = self;
  this->quality->delegate.didSelectOption = didSelectQuality;

  $(this->quality, selectOptionWithValue, (ident) detectQualityPreset());

  int32_t count = 0;
  SDL_AudioDeviceID *devices = SDL_GetAudioPlaybackDevices(&count);
  addAudioDevices(playbackDevice, devices, count);

  devices = SDL_GetAudioRecordingDevices(&count);
  addAudioDevices(captureDevice, devices, count);

  apply->delegate.didClick = didClickApply;
  apply->delegate.self = self;

  refreshQualityControls(this);
}

static void viewWillAppear(ViewController *self) {
  super(ViewController, self, viewWillAppear);
  refreshQualityControls((SettingsViewController *) self);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;
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
      .interfaceSize = sizeof(SettingsViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
