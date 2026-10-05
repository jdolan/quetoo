/*
 * Copyright(c) 2026 Quetoo.
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

#include "CaptureLevelView.h"

#define _Class _CaptureLevelView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  CaptureLevelView *this = (CaptureLevelView *) self;

  release(this->inputCaption);
  release(this->outputCaption);

  super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((CaptureLevelView *) self, initWithFrame, NULL);
}

/**
 * @see View::applyStyle(View *, const Style *)
 */
static void applyStyle(View *self, const Style *style) {

  super(View, self, applyStyle, style);

  CaptureLevelView *this = (CaptureLevelView *) self;

  const Inlet inlets[] = MakeInlets(
    MakeInlet("-level-color", InletTypeColor, &this->levelColor, NULL),
    MakeInlet("-track-color", InletTypeColor, &this->trackColor, NULL),
    MakeInlet("-threshold-color", InletTypeColor, &this->thresholdColor, NULL),
    MakeInlet("-clipping-color", InletTypeColor, &this->clippingColor, NULL)
  );

  $(self, bind, inlets, style->attributes);
}

/**
 * @brief Maps normalized RMS to a -80 to 0 dBFS meter, so quiet microphones remain visible.
 */
static float normalize(float level) {
  return level > 0.f ? Clampf01((20.f * log10f(level) + 80.f) / 80.f) : 0.f;
}

/**
 * @brief Returns the meter track beside the specified caption.
 */
static SDL_Rect frameForTrack(const View *self, const Text *caption) {

  const SDL_Rect frame = $(self, renderFrame);
  const SDL_Rect label = $((View *) caption, renderFrame);

  const int32_t x = label.x + label.w + 8;

  return (SDL_Rect) { x, label.y + 3, max(0, frame.x + frame.w - x), max(0, label.h - 6) };
}

/**
 * @brief Draws a meter track and its logarithmic level fill.
 */
static void drawLevel(const CaptureLevelView *self, Renderer *renderer, SDL_Rect track, float level, const SDL_Color *color) {

  $(renderer, drawRectFilled, &track, &self->trackColor);

  track.w = (int32_t) lroundf(track.w * normalize(level));
  if (track.w > 0 && track.h > 0) {
    $(renderer, drawRectFilled, &track, color);
  }
}

/**
 * @see View::render(View *, Renderer *)
 */
static void render(View *self, Renderer *renderer) {

  super(View, self, render, renderer);

  CaptureLevelView *this = (CaptureLevelView *) self;

  const SoundCaptureLevel level = cgi.CaptureLevel();
  const uint64_t now = SDL_GetTicks();
  const float decay = expf(-(float) (now - this->timestamp) / 180.f);

  this->timestamp = now;

  this->input = level.capturing ? max(level.input, this->input * decay) : 0.f;
  this->output = level.capturing ? max(level.output, this->output * decay) : 0.f;

  const SDL_Rect input = frameForTrack(self, this->inputCaption);
  const SDL_Rect output = frameForTrack(self, this->outputCaption);

  drawLevel(this, renderer, input, this->input, &this->levelColor);
  drawLevel(this, renderer, output, this->output, level.peak >= 0.99f ? &this->clippingColor : &this->levelColor);

  if (input.w > 0 && input.h > 0) {
    const int32_t offset = (int32_t) lroundf((input.w - 1) * normalize(level.threshold));
    const SDL_Rect marker = { input.x + offset, input.y - 2, 1, input.h + 4 };
    $(renderer, drawRectFilled, &marker, &this->thresholdColor);
  }
}

#pragma mark - CaptureLevelView

/**
 * @fn CaptureLevelView *CaptureLevelView::initWithFrame(CaptureLevelView *self, const SDL_Rect *frame)
 * @memberof CaptureLevelView
 */
static CaptureLevelView *initWithFrame(CaptureLevelView *self, const SDL_Rect *frame) {

  self = (CaptureLevelView *) super(View, self, initWithFrame, frame);
  if (self) {

    self->inputCaption = $(alloc(Text), initWithText, "Input", NULL);
    self->outputCaption = $(alloc(Text), initWithText, "Output", NULL);
    assert(self->inputCaption);
    assert(self->outputCaption);

    $((View *) self->inputCaption, addClassName, "inputCaption");
    $((View *) self->outputCaption, addClassName, "outputCaption");
    $((View *) self, addSubview, (View *) self->inputCaption);
    $((View *) self, addSubview, (View *) self->outputCaption);
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
  ((ViewInterface *) clazz->interface)->applyStyle = applyStyle;
  ((ViewInterface *) clazz->interface)->render = render;

  ((CaptureLevelViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *CaptureLevelView::_CaptureLevelView(void)
 * @memberof CaptureLevelView
 */
Class *_CaptureLevelView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "CaptureLevelView",
      .superclass = _View(),
      .instanceSize = sizeof(CaptureLevelView),
      .interfaceSize = sizeof(CaptureLevelViewInterface),
      .initialize = initialize,
    });

  });
  return clazz;
}

#undef _Class
