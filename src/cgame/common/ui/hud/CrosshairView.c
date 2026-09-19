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

#include "CrosshairView.h"

#define _Class _CrosshairView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  CrosshairView *this = (CrosshairView *) self;

  release(this->imageView);

  super(Object, self, dealloc);
}

#pragma mark - Health

/**
 * @brief Applies the `cg_drawCrosshairHealth` scheme to the RGB of `color`.
 */
static void applyHealth(Vec4 *color, int16_t health) {

  const float frac = Clampf01(health / 100.f);
  const float over = Clampf01((health - 100) / 100.f);
  const float low = Clampf01((health - 15) / 50.f);
  const float medium = Clampf01((health - 65) / 35.f);

  switch (cg_drawCrosshairHealth->integer) {
    case CROSSHAIR_HEALTH_RED_WHITE:
      color->x = 1.f;
      color->y = frac;
      color->z = frac;
      break;
    case CROSSHAIR_HEALTH_RED_WHITE_GREEN:
      if (health <= 100) {
        color->x = 1.f;
        color->y = frac;
        color->z = frac;
      } else {
        color->x = 1.f - over;
        color->y = 1.f;
        color->z = 1.f - over;
      }
      break;
    case CROSSHAIR_HEALTH_RED_YELLOW_WHITE:
      if (health <= 20) {
        color->x = 1.f;
        color->y = 0.f;
        color->z = 0.f;
      } else if (health <= 70) {
        color->x = 1.f;
        color->y = low;
        color->z = 0.f;
      } else {
        color->x = 1.f;
        color->y = 1.f;
        color->z = medium;
      }
      break;
    case CROSSHAIR_HEALTH_RED_YELLOW_WHITE_GREEN:
      if (health <= 20) {
        color->x = 1.f;
        color->y = 0.f;
        color->z = 0.f;
      } else if (health <= 70) {
        color->x = 1.f;
        color->y = low;
        color->z = 0.f;
      } else if (health <= 100) {
        color->x = 1.f;
        color->y = 1.f;
        color->z = medium;
      } else {
        color->x = 1.f - over;
        color->y = 1.f;
        color->z = 1.f - over;
      }
      break;
    case CROSSHAIR_HEALTH_WHITE_GREEN:
      if (health <= 100) {
        color->x = 1.f;
        color->y = 1.f;
        color->z = 1.f;
      } else {
        color->x = 1.f - over;
        color->y = 1.f;
        color->z = 1.f - over;
      }
      break;
    default:
      break;
  }
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = super(View, self, initWithFrame, NULL);
  if (self) {
    CrosshairView *this = (CrosshairView *) self;

    this->color = Vec4_One();

    this->imageView = $(alloc(ImageView), initWithFrame, NULL);
    assert(this->imageView);

    $(self, addSubview, (View *) this->imageView);
  }

  return self;
}

/**
 * @brief Whether the crosshair is drawn for the given player state.
 */
static bool visible(const PlayerState *ps) {

  if (editor->value) {
    return true;
  }

  if (!cg_drawCrosshair->value) {
    return false;
  }

  if (ps->stats[STAT_SCORES]) {
    return false;
  }

  if (cgi.client->thirdPerson) {
    return false;
  }

  if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
    return false;
  }

  if (ps->pmState.type == PM_DEAD) {
    return false;
  }

  if (!Cg_HasWeapon(ps)) {
    return false;
  }

  if (cgState.centerPrint.time > cgi.client->unclampedTime) {
    return false;
  }

  return true;
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  CrosshairView *this = (CrosshairView *) self;

  if (data == NULL) {
    cg_drawCrosshair->modified = true;
    cg_drawCrosshairScale->modified = true;
    cg_drawCrosshairColor->modified = true;
    return;
  }

  const PlayerState *ps = &((const ClientFrame *) data)->ps;

  if (cg_drawCrosshair->modified || cg_drawCrosshairScale->modified) {
    cg_drawCrosshair->modified = false;
    cg_drawCrosshairScale->modified = false;

    cg_drawCrosshair->value = Clampf(cg_drawCrosshair->value, 0.f, 100.f);
    cg_drawCrosshairScale->value = Clampf(cg_drawCrosshairScale->value, 0.f, 4.f);

    $(this->imageView, setImage, NULL);

    if (cg_drawCrosshair->integer) {
      const float scale = cg_drawCrosshairScale->value * CROSSHAIR_SCALE;

      Image *image = Cg_LoadImageScaled(va("pics/ch%d", cg_drawCrosshair->integer), scale);
      if (image) {
        $(this->imageView, setImage, image);
        release(image);
      } else {
        Cg_Warn("Couldn't load pics/ch%d\n", cg_drawCrosshair->integer);
      }
    }
  }

  if (cg_drawCrosshairColor->modified) {
    cg_drawCrosshairColor->modified = false;

    Color color = color_white;
    if (q_strcmp(cg_drawCrosshairColor->string, "default")) {
      if (!Color_Parse(cg_drawCrosshairColor->string, &color)) {
        color = color_white;
      }
    }

    this->color = color.vec4;
  }

  const bool shown = this->imageView->image && visible(ps);

  $(self, setVisibility, shown ? ViewVisibilityVisible : ViewVisibilityHidden);

  if (!shown) {
    return;
  }

  Vec4 color = this->color;

  applyHealth(&color, ps->stats[STAT_HEALTH]);

  float scale = cg_drawCrosshairScale->value * CROSSHAIR_SCALE;

  if (cg_drawCrosshairPulse->value) {

    const int16_t p = ps->stats[STAT_PICKUP];
    if (p && p != cgHudState.pulse.pickup) {
      cgHudState.pulse.time = cgi.client->unclampedTime;
    }

    cgHudState.pulse.pickup = p;

    const uint32_t delta = cgi.client->unclampedTime - cgHudState.pulse.time;
    if (delta < 300) {
      const float frac = delta / 300.f;
      scale += sinf(frac * M_PI) * CROSSHAIR_SCALE;
      color.w += sinf((frac - 1.f) * M_PI) * CROSSHAIR_PULSE_ALPHA;
    }
  }

  color.w *= cg_drawCrosshairAlpha->value;

  if (editor->value) {
    color = Vec4_One();
  }

  const Color32 rgba = Color_Color32(Color4fv(color));
  this->imageView->color = (SDL_Color) { rgba.r, rgba.g, rgba.b, rgba.a };

  const SDL_Size imageSize = $(this->imageView->image, size);
  const SDL_Size size = MakeSize(imageSize.w * scale, imageSize.h * scale);

  if (size.w != this->imageView->view.frame.w || size.h != this->imageView->view.frame.h) {
    $((View *) this->imageView, resize, &size);
  }
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

/**
 * @fn Class *CrosshairView::_CrosshairView(void)
 * @memberof CrosshairView
 */
Class *_CrosshairView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "CrosshairView",
      .superclass = _View(),
      .instanceSize = sizeof(CrosshairView),
      .interfaceSize = sizeof(CrosshairViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
