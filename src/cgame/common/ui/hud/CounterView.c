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

#include "CounterView.h"

#define _Class _CounterView

static const EnumName CounterViewStatNames[] = MakeEnumNames(
  MakeEnumAlias(STAT_FRAGS, frags),
  MakeEnumAlias(STAT_DEATHS, deaths)
);

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  CounterView *this = (CounterView *) self;

  release(this->caption);
  release(this->value);

  super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::awakeWithDictionary(View *, const Dictionary *)
 */
static void awakeWithDictionary(View *self, const Dictionary *dictionary) {

  super(View, self, awakeWithDictionary, dictionary);

  CounterView *this = (CounterView *) self;

  char *caption = NULL;

  const Inlet inlets[] = MakeInlets(
    MakeInlet("caption", InletTypeCharacters, &caption, NULL),
    MakeInlet("stat", InletTypeEnum, &this->stat, (ident) CounterViewStatNames)
  );

  $(self, bind, inlets, dictionary);

  if (caption) {
    $(this->caption, setText, caption);
    free(caption);
  }
}

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((CounterView *) self, initWithCaption, NULL, STAT_FRAGS);
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  if (data == NULL) {
    return;
  }

  CounterView *this = (CounterView *) self;

  const cl_frame_t *frame = data;
  const player_state_t *ps = &frame->ps;

  if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
    $(this->value, setText, " ");
  } else {
    const int32_t value = $(this, valueForFrame, frame);
    $(this->value, setTextWithFormat, "%3d", value);
  }
}

#pragma mark - CounterView

/**
 * @fn CounterView *CounterView::initWithCaption(CounterView *self, const char *caption, int32_t stat)
 * @memberof CounterView
 */
static CounterView *initWithCaption(CounterView *self, const char *caption, int32_t stat) {

  self = (CounterView *) super(StackView, self, initWithFrame, NULL);
  if (self) {
    self->stat = stat;

    self->stackView.axis = StackViewAxisVertical;

    self->caption = $(alloc(Text), initWithText, caption, NULL);
    assert(self->caption);

    $((View *) self->caption, addClassName, "caption");
    $((View *) self, addSubview, (View *) self->caption);

    self->value = $(alloc(Text), initWithText, " ", NULL);
    assert(self->value);

    $((View *) self->value, addClassName, "value");
    $((View *) self, addSubview, (View *) self->value);
  }

  return self;
}

static int32_t valueForFrame(CounterView *self, const cl_frame_t *frame) {
  return frame->ps.stats[self->stat];
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->awakeWithDictionary = awakeWithDictionary;
  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

  ((CounterViewInterface *) clazz->interface)->initWithCaption = initWithCaption;
  ((CounterViewInterface *) clazz->interface)->valueForFrame = valueForFrame;
}

/**
 * @fn Class *CounterView::_CounterView(void)
 * @memberof CounterView
 */
Class *_CounterView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "CounterView",
      .superclass = _StackView(),
      .instanceSize = sizeof(CounterView),
      .interfaceSize = sizeof(CounterViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
