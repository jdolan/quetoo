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

#include "CrosshairPreviewView.h"

#define _Class _CrosshairPreviewView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  CrosshairPreviewView *this = (CrosshairPreviewView *) self;

  release(this->imageView);

  super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((CrosshairPreviewView *) self, initWithFrame, NULL);
}

/**
 * @see View::layoutSubviews(View *)
 */
static void layoutSubviews(View *self) {

  CrosshairPreviewView *this = (CrosshairPreviewView *) self;
  if (this->imageView->image) {

    const float scale = cg_draw_crosshair_scale->value * CROSSHAIR_SCALE;

    const SDL_Size size = MakeSize(
      this->imageView->image->surface->w * scale,
      this->imageView->image->surface->h * scale
    );

    $((View *) this->imageView, resize, &size);
    $((View *) this->imageView, layoutIfNeeded);
  }

  super(View, self, layoutSubviews);
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  CrosshairPreviewView *this = (CrosshairPreviewView *) self;

  $(this->imageView, setImage, NULL);

  const int32_t ch = cg_draw_crosshair->value;
  if (ch) {
    SDL_Surface *surface = cgi.LoadSurface(va("pics/ch%d", ch));
    if (surface) {

      $(this->imageView, setImageWithSurface, surface);
      SDL_DestroySurface(surface);

      SDL_Color color = Colors.White;
      if (q_strcmp(cg_draw_crosshair_color->string, "default")) {
        color = MVC_HexToRGBA(cg_draw_crosshair_color->string);
        if (color.r == 0 && color.g == 0 && color.b == 0) {
          color = Colors.White;
        }
      }

      this->imageView->color.r = color.r;
      this->imageView->color.g = color.g;
      this->imageView->color.b = color.b;
      this->imageView->color.a = Clampf(cg_draw_crosshair_alpha->value * 255, 0, 255);
    }
  }

  self->needsLayout = true;
}

#pragma mark - CrosshairPreviewView

/**
 * @fn CrosshairPreviewView *CrosshairPreviewView::initWithFrame(CrosshairPreviewView *self, const SDL_Rect *frame)
 *
 * @memberof CrosshairPreviewView
 */
static CrosshairPreviewView *initWithFrame(CrosshairPreviewView *self, const SDL_Rect *frame) {

  self = (CrosshairPreviewView *) super(Control, self, initWithFrame, frame);
  if (self) {

    self->imageView = $(alloc(ImageView), initWithFrame, NULL);
    assert(self->imageView);

    $((View *) self, addSubview, (View *) self->imageView);
  }

  return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->layoutSubviews = layoutSubviews;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

  ((CrosshairPreviewViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *CrosshairPreviewView::_CrosshairPreviewView(void)
 * @memberof CrosshairPreviewView
 */
Class *_CrosshairPreviewView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "CrosshairPreviewView",
      .superclass = _Control(),
      .instanceSize = sizeof(CrosshairPreviewView),
      .interfaceSize = sizeof(CrosshairPreviewViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
