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

#include "Scrollbar.h"

#define _Class _Scrollbar

// Fallback defaults, used only if the common.css `.scrollbar` rule is absent.
// The live/tunable values are these fields bound from CSS in applyStyle.
#define SCROLLBAR_ADORNER_SIZE 12
#define SCROLLBAR_ADORNER_SHADE 28
#define SCROLLBAR_STEP 32
#define SCROLLBAR_MIN_THUMB 16

const EnumName ScrollbarOrientationNames[] = MakeEnumNames(
  MakeEnumAlias(ScrollbarOrientationRight, right),
  MakeEnumAlias(ScrollbarOrientationLeft, left)
);

#pragma mark - Helpers

/**
 * @return `c` brightened by `delta` per channel (clamped), preserving alpha.
 */
static SDL_Color shade(const SDL_Color c, const int delta) {
  return (SDL_Color) {
    .r = (Uint8) clamp(c.r + delta, 0, 255),
    .g = (Uint8) clamp(c.g + delta, 0, 255),
    .b = (Uint8) clamp(c.b + delta, 0, 255),
    .a = c.a
  };
}

/**
 * @brief Scrolls the bound content by `dy` pixels and re-syncs the thumb.
 */
static void scrollBy(Scrollbar *self, const int dy) {

  if (self->scrollView) {
    SDL_Point offset = self->scrollView->contentOffset;
    offset.y += dy;
    $(self->scrollView, scrollToOffset, &offset);
    $(self, update);

    // Force a full bar re-layout so the adorner frames are re-asserted. A click
    // restyles the adorner Button (invalidateStyle + needsLayout), and the base
    // theme's `Button { autoresizing-mask: contain }` would otherwise shrink it to
    // its empty content on that self-layout. layoutSubviews (below) neutralizes the
    // mask and re-frames it.
    ((View *) self)->needsLayout = true;
  }
}

#pragma mark - Delegate callbacks

/**
 * @brief ScrollThumbDelegate; maps thumb travel onto the content offset.
 */
static void thumbDidDrag(ScrollThumb *thumb, int delta) {

  Scrollbar *self = thumb->delegate.self;

  if (self->scrollView && self->scrollView->contentView) {

    const SDL_Rect panel = $(self->scrollPanel, bounds);
    const SDL_Size content = $(self->scrollView->contentView, size);
    const SDL_Rect visible = $((View *) self->scrollView, bounds);

    const int scrollRange = content.h - visible.h;
    const int travel = panel.h - ((View *) self->thumb)->frame.h;

    if (scrollRange > 0 && travel > 0) {
      scrollBy(self, -(int) (delta * ((float) scrollRange / travel)));
    }
  }
}

/**
 * @brief ButtonDelegate; the top cap steps the content toward its start.
 */
static void didClickTopAdorner(Button *button) {
  Scrollbar *self = button->delegate.self;
  scrollBy(self, self->step);
}

/**
 * @brief ButtonDelegate; the bottom cap steps the content toward its end.
 */
static void didClickBottomAdorner(Button *button) {
  Scrollbar *self = button->delegate.self;
  scrollBy(self, -self->step);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  Scrollbar *this = (Scrollbar *) self;

  release(this->topAdorner);
  release(this->scrollPanel);
  release(this->bottomAdorner);
  release(this->thumb);

  super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::applyStyle(View *, const Style *)
 */
static void applyStyle(View *self, const Style *style) {

  super(View, self, applyStyle, style);

  Scrollbar *this = (Scrollbar *) self;

  const Inlet inlets[] = MakeInlets(
    MakeInlet("-adorner-size", InletTypeInteger, &this->adornerSize, NULL),
    MakeInlet("-adorner-shade", InletTypeInteger, &this->adornerShade, NULL),
    MakeInlet("-orientation", InletTypeEnum, &this->orientation, (ident) ScrollbarOrientationNames),
    MakeInlet("-bgcolor", InletTypeColor, &this->bgColor, NULL),
    MakeInlet("-fgcolor", InletTypeColor, &this->fgColor, NULL),
    MakeInlet("-thumb-min", InletTypeInteger, &this->thumbMin, NULL),
    MakeInlet("-step", InletTypeInteger, &this->step, NULL)
  );

  $(self, bind, inlets, (Dictionary *) style->attributes);

  self->backgroundColor = this->bgColor;

  const SDL_Color adorn = shade(this->bgColor, this->adornerShade);
  ((View *) this->topAdorner)->backgroundColor = adorn;
  ((View *) this->bottomAdorner)->backgroundColor = adorn;
  ((View *) this->thumb)->backgroundColor = this->fgColor;

  self->needsLayout = true;
}

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((Scrollbar *) self, initWithScrollView, NULL);
}

/**
 * @see View::layoutSubviews(View *)
 */
static void layoutSubviews(View *self) {

  Scrollbar *this = (Scrollbar *) self;

  // The base theme styles our adorner Buttons with `autoresizing-mask: contain`,
  // which makes them self-shrink to their (empty) content -- clamped to the base
  // `min-width: 100` -- on any self-layout. We frame the adorners by hand, so
  // neutralize that mask before the base layout pass runs it.
  ((View *) this->topAdorner)->autoresizingMask = ViewAutoresizingNone;
  ((View *) this->bottomAdorner)->autoresizingMask = ViewAutoresizingNone;

  super(View, self, layoutSubviews);

  const SDL_Rect bounds = $(self, bounds);
  const int a = this->adornerSize;

  ((View *) this->topAdorner)->frame = (SDL_Rect) { 0, 0, bounds.w, a };
  this->scrollPanel->frame = (SDL_Rect) { 0, a, bounds.w, max(0, bounds.h - 2 * a) };
  ((View *) this->bottomAdorner)->frame = (SDL_Rect) { 0, bounds.h - a, bounds.w, a };

  $(this, update);
}

/**
 * @see View::render(View *, Renderer *)
 */
static void render(View *self, Renderer *renderer) {

  Scrollbar *this = (Scrollbar *) self;

  // Re-sync the thumb from the scroll view's geometry every frame. The base
  // ScrollView fires no didScroll callback (so wheel/drag offset changes are only
  // reflected here), and the content view's height is often not final on the
  // Scrollbar's first layout -- so computing the thumb only in layoutSubviews left
  // it stuck at full height until something (e.g. an adorner click) forced a
  // re-layout. update() is cheap and idempotent.
  if (this->scrollView) {
    $(this, update);
  }

  super(View, self, render, renderer);
}

#pragma mark - Scrollbar

/**
 * @fn Scrollbar *Scrollbar::initWithScrollView(Scrollbar *self, ScrollView *scrollView)
 * @memberof Scrollbar
 */
static Scrollbar *initWithScrollView(Scrollbar *self, ScrollView *scrollView) {

  self = (Scrollbar *) super(View, self, initWithFrame, NULL);
  if (self) {

    $((View *) self, addClassName, "scrollbar");

    self->adornerSize = SCROLLBAR_ADORNER_SIZE;
    self->adornerShade = SCROLLBAR_ADORNER_SHADE;
    self->orientation = ScrollbarOrientationRight;
    self->bgColor = (SDL_Color) { 0x22, 0x22, 0x22, 0xc0 };
    self->fgColor = (SDL_Color) { 0x1e, 0x4e, 0x62, 0xdd };
    self->thumbMin = SCROLLBAR_MIN_THUMB;
    self->step = SCROLLBAR_STEP;

    self->topAdorner = $(alloc(Button), initWithFrame, NULL);
    assert(self->topAdorner);
    $((View *) self->topAdorner, addClassName, "adorner");
    self->topAdorner->delegate.self = self;
    self->topAdorner->delegate.didClick = didClickTopAdorner;
    $((View *) self, addSubview, (View *) self->topAdorner);

    self->scrollPanel = $(alloc(View), initWithFrame, NULL);
    assert(self->scrollPanel);
    $(self->scrollPanel, addClassName, "scrollPanel");
    self->scrollPanel->clipsSubviews = true;
    $((View *) self, addSubview, self->scrollPanel);

    self->bottomAdorner = $(alloc(Button), initWithFrame, NULL);
    assert(self->bottomAdorner);
    $((View *) self->bottomAdorner, addClassName, "adorner");
    self->bottomAdorner->delegate.self = self;
    self->bottomAdorner->delegate.didClick = didClickBottomAdorner;
    $((View *) self, addSubview, (View *) self->bottomAdorner);

    self->thumb = $(alloc(ScrollThumb), initWithFrame, NULL);
    assert(self->thumb);
    self->thumb->delegate.self = self;
    self->thumb->delegate.didDrag = thumbDidDrag;
    $(self->scrollPanel, addSubview, (View *) self->thumb);

    $(self, setScrollView, scrollView);
  }

  return self;
}

/**
 * @fn void Scrollbar::setScrollView(Scrollbar *self, ScrollView *scrollView)
 * @memberof Scrollbar
 */
static void setScrollView(Scrollbar *self, ScrollView *scrollView) {

  self->scrollView = scrollView;
  ((View *) self)->needsLayout = true;
}

/**
 * @fn void Scrollbar::update(Scrollbar *self)
 * @memberof Scrollbar
 */
static void update(Scrollbar *self) {

  if (self->scrollView == NULL || self->scrollView->contentView == NULL) {
    return;
  }

  const SDL_Rect panel = $(self->scrollPanel, bounds);
  const SDL_Size content = $(self->scrollView->contentView, size);
  const SDL_Rect visible = $((View *) self->scrollView, bounds);

  View *thumb = (View *) self->thumb;

  if (content.h <= visible.h || panel.h <= 0) {
    thumb->frame = (SDL_Rect) { 0, 0, panel.w, panel.h };
  } else {

    const int scrollRange = content.h - visible.h;

    int thumbH = (int) ((float) panel.h * visible.h / content.h);
    thumbH = clamp(thumbH, self->thumbMin, panel.h);

    const int travel = panel.h - thumbH;
    const float fraction = clamp((float) (-self->scrollView->contentOffset.y) / scrollRange, 0.f, 1.f);

    thumb->frame = (SDL_Rect) { 0, (int) (fraction * travel), panel.w, thumbH };
  }

  self->syncedOffset = self->scrollView->contentOffset;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->applyStyle = applyStyle;
  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->layoutSubviews = layoutSubviews;
  ((ViewInterface *) clazz->interface)->render = render;

  ((ScrollbarInterface *) clazz->interface)->initWithScrollView = initWithScrollView;
  ((ScrollbarInterface *) clazz->interface)->setScrollView = setScrollView;
  ((ScrollbarInterface *) clazz->interface)->update = update;
}

/**
 * @fn Class *Scrollbar::_Scrollbar(void)
 * @memberof Scrollbar
 */
Class *_Scrollbar(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "Scrollbar",
      .superclass = _View(),
      .instanceSize = sizeof(Scrollbar),
      .interfaceOffset = offsetof(Scrollbar, interface),
      .interfaceSize = sizeof(ScrollbarInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
