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

#include "BlendView.h"

#define _Class _BlendView

#define BLEND_DAMAGE_TIME 1500
#define BLEND_PICKUP_TIME 600

static const char *BlendViewFlashImages[BlendViewTotal] = {
  "pics/pickup",
  "pics/powerup_quad",
  "pics/powerup_invisibility",
  "pics/powerup_invulnerability",
  "pics/damage"
};

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  BlendView *this = (BlendView *) self;

  for (size_t i = 0; i < BlendViewTotal; i++) {
    release(this->flashes[i]);
  }

  super(Object, self, dealloc);
}

#pragma mark - Blends

/**
 * @brief The alpha of a flash that began at `start` and fades over `decay`.
 */
static float decayingAlpha(uint32_t start, uint32_t decay, float alpha) {

  const uint32_t elapsed = cgi.client->unclamped_time - start;
  if (start && elapsed <= decay) {
    return cg_draw_blend->value * alpha * (1.f - elapsed / (float) decay);
  }

  return 0.f;
}

/**
 * @brief The alpha of a powerup glow, pulsing.
 */
static float pulsingAlpha(void) {
  return fabsf(sinf(Radians(cgi.client->unclamped_time * 0.2))) * cg_draw_blend_powerup->value;
}

/**
 * @brief The tint of the liquid the view origin is in, or transparent.
 */
static SDL_Color liquidTint(void) {

  const int32_t contents = cgi.view->contents;

  if (!(contents & CONTENTS_MASK_LIQUID) || !cg_draw_blend_liquid->value) {
    return Colors.Transparent;
  }

  color_t color;

  const cm_trace_t tr = cgi.Trace(cgi.view->origin, cgi.view->origin, Box3_Zero(), NULL, CONTENTS_MASK_LIQUID);
  if (tr.brush) {
    const char *name = tr.brush->brush_sides[0].material->name;
    color = cgi.LoadMaterial(name, ASSET_CONTEXT_TEXTURES)->color;
    const float f = Maxf(color.r, Maxf(color.g, color.b));
    if (f > 0.f) {
      color = Color_Scale(color, 1.f / f);
    }
  } else if (contents & CONTENTS_LAVA) {
    color = Color4f(.8f, .4f, .1f, 1.f);
  } else if (contents & CONTENTS_SLIME) {
    color = Color4f(.4f, .7f, .2f, 1.f);
  } else {
    color = Color4f(.4f, .5f, .6f, 1.f);
  }

  color.a = Clampf(cg_draw_blend_liquid->value * 0.4f, 0.f, 0.4f);

  const color32_t rgba = Color_Color32(color);
  return (SDL_Color) { rgba.r, rgba.g, rgba.b, rgba.a };
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = $(self, initWithFrame, NULL);
  if (self) {
    BlendView *this = (BlendView *) self;

    for (size_t i = 0; i < BlendViewTotal; i++) {
      this->flashes[i] = $(alloc(ImageView), initWithFrame, NULL);
      assert(this->flashes[i]);

      this->flashes[i]->view.autoresizingMask = ViewAutoresizingFill;

      $(self, addSubview, (View *) this->flashes[i]);
    }
  }

  return self;
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  BlendView *this = (BlendView *) self;

  if (data == NULL) {
    for (size_t i = 0; i < BlendViewTotal; i++) {
      SDL_Surface *surface = cgi.LoadSurface(BlendViewFlashImages[i]);
      if (surface) {
        $(this->flashes[i], setImageWithSurface, surface);
        SDL_DestroySurface(surface);
      } else {
        Cg_Warn("Failed to load %s\n", BlendViewFlashImages[i]);
      }
    }
    return;
  }

  const player_state_t *ps = &((const cl_frame_t *) data)->ps;

  $(self, setVisibility, cg_draw_blend->value ? ViewVisibilityVisible : ViewVisibilityHidden);

  if (!cg_draw_blend->value) {
    return;
  }

  self->backgroundColor = liquidTint();

  const int16_t pickup = ps->stats[STAT_PICKUP] & ~STAT_TOGGLE_BIT;
  if (pickup && pickup != cg_hud_state.blend.pickup) {
    cg_hud_state.blend.pickup_time = cgi.client->unclamped_time;
  }
  cg_hud_state.blend.pickup = pickup;

  if (ps->stats[STAT_DAMAGE_ARMOR] + ps->stats[STAT_DAMAGE_HEALTH]) {
    cg_hud_state.blend.damage_time = cgi.client->unclamped_time;
  }

  float alphas[BlendViewTotal] = {
    [BlendViewPickup] = cg_draw_blend_pickup->value ? decayingAlpha(cg_hud_state.blend.pickup_time, BLEND_PICKUP_TIME, cg_draw_blend_pickup->value) : 0.f,
    [BlendViewQuad] = ps->stats[STAT_QUAD_TIME] > 0 ? pulsingAlpha() : 0.f,
    [BlendViewInvisibility] = ps->stats[STAT_INVISIBILITY_TIME] > 0 ? pulsingAlpha() : 0.f,
    [BlendViewInvulnerability] = ps->stats[STAT_INVULNERABILITY_TIME] > 0 ? pulsingAlpha() : 0.f,
    [BlendViewDamage] = cg_draw_blend_damage->value ? decayingAlpha(cg_hud_state.blend.damage_time, BLEND_DAMAGE_TIME, cg_draw_blend_damage->value) : 0.f,
  };

  for (size_t i = 0; i < BlendViewTotal; i++) {
    const float alpha = Clampf01(alphas[i]);

    $((View *) this->flashes[i], setVisibility,
      alpha <= 0.f ? ViewVisibilityHidden : ViewVisibilityVisible);
    this->flashes[i]->color.a = (Uint8) (alpha * 255);
  }
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

/**
 * @fn Class *BlendView::_BlendView(void)
 * @memberof BlendView
 */
Class *_BlendView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "BlendView",
      .superclass = _View(),
      .instanceSize = sizeof(BlendView),
      .interfaceSize = sizeof(BlendViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
