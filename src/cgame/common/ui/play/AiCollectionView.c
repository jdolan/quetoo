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

#include "AiCollectionView.h"

#define _Class _AiCollectionView

/**
 * @brief The roster file, in the map list format: one entity of keys for each AI player.
 */
#define AI_ROSTER_FILE "ai.lst"

#pragma mark - CollectionViewDataSource

/**
 * @see CollectionViewDataSource::numberOfItems(const CollectionView *)
 */
static size_t numberOfItems(const CollectionView *collectionView) {
  return ((const AiCollectionView *) collectionView)->numShown;
}

/**
 * @see CollectionViewDataSource::objectForItemAtIndexPath(const CollectionView *, const IndexPath *)
 */
static ident objectForItemAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

  const AiCollectionView *this = (const AiCollectionView *) collectionView;

  return &this->ai[this->shown[$(indexPath, indexAtPosition, 0)]];
}

#pragma mark - CollectionViewDelegate

/**
 * @see CollectionViewDelegate::itemForObjectAtIndexPath(const CollectionView *, const IndexPath *)
 */
static CollectionItemView *itemForObjectAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

  const AiCollectionView *this = (const AiCollectionView *) collectionView;

  AiCollectionItemView *item = $(alloc(AiCollectionItemView), initWithFrame, NULL);
  assert(item);

  const size_t index = $(indexPath, indexAtPosition, 0);

  $(item, setAiItemInfo, &this->ai[this->shown[index]]);

  return (CollectionItemView *) item;
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  AiCollectionView *this = (AiCollectionView *) self;

  free(this->ai);
  free(this->shown);

  super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((AiCollectionView *) self, initWithFrame, NULL);
}

#pragma mark - AiCollectionView

/**
 * @brief Orders AI players by skill, then by name.
 */
static int AiCollectionView_compare(const void *a, const void *b) {

  const AiItemInfo *x = a, *y = b;

  if (x->skill != y->skill) {
    return x->skill < y->skill ? -1 : 1;
  }

  return Str_CaseCompare(x->name, y->name);
}

/**
 * @brief Loads the AI players of `ai.lst`. An entry without a name or skin, or with a trait that
 * is not a number from 0 to 1, is skipped with a warning, as the game skips it.
 */
static void AiCollectionView_load(AiCollectionView *self) {

  void *buffer;
  if (cgi.LoadFile(AI_ROSTER_FILE, &buffer) == -1) {
    Cg_Warn("Couldn't load %s\n", AI_ROSTER_FILE);
    return;
  }

  List *entities = cgi.LoadEntities(buffer);
  cgi.FreeFile(buffer);

  self->ai = calloc(Maxi(1, (int32_t) entities->count), sizeof(AiItemInfo));
  assert(self->ai);

  int32_t index = 0;
  for (const ListNode *node = entities->head; node; node = node->next, index++) {
    const Entity *e = node->element;

    const char *name = cgi.EntityValue(e, "name")->string;
    const char *skin = cgi.EntityValue(e, "skin")->string;

    if (!*name || !*skin) {
      Cg_Warn("%s: entry %d has no name or no skin, and is skipped\n", AI_ROSTER_FILE, index);
      continue;
    }

    AiItemInfo *info = &self->ai[self->numAi];

    Str_Copy(info->name, name, sizeof(info->name));
    Str_Copy(info->skin, skin, sizeof(info->skin));

    const char *traits[] = { "skill", "aggression", "awareness" };
    float *values[] = { &info->skill, &info->aggression, &info->awareness };

    bool valid = true;
    for (size_t i = 0; i < lengthof(traits); i++) {
      const char *string = cgi.EntityValue(e, traits[i])->string;
      char *end;
      const float value = strtof(string, &end);
      if (end == string || *end || !(value >= 0.f && value <= 1.f)) {
        Cg_Warn("%s: %s has %s \"%s\", which is not a number from 0 to 1, and is skipped\n",
                AI_ROSTER_FILE, name, traits[i], string);
        valid = false;
        break;
      }
      *values[i] = value;
    }

    if (valid) {
      self->numAi++;
    }
  }

  for (const ListNode *node = entities->head; node; node = node->next) {
    cgi.FreeEntity(node->element);
  }

  release(entities);

  qsort(self->ai, self->numAi, sizeof(AiItemInfo), AiCollectionView_compare);
}

/**
 * @fn AiCollectionView *AiCollectionView::initWithFrame(AiCollectionView *self, const SDL_Rect *frame)
 * @memberof AiCollectionView
 */
static AiCollectionView *initWithFrame(AiCollectionView *self, const SDL_Rect *frame) {

  self = (AiCollectionView *) super(CollectionView, self, initWithFrame, frame);
  if (self) {
    AiCollectionView_load(self);

    self->shown = calloc(Maxi(1, (int32_t) self->numAi), sizeof(size_t));
    assert(self->shown);

    self->collectionView.dataSource.numberOfItems = numberOfItems;
    self->collectionView.dataSource.objectForItemAtIndexPath = objectForItemAtIndexPath;
    self->collectionView.delegate.itemForObjectAtIndexPath = itemForObjectAtIndexPath;

    $(self, setNames, NULL);
  }

  return self;
}

/**
 * @fn PointerArray *AiCollectionView::selectedAi(const AiCollectionView *self)
 * @memberof AiCollectionView
 */
static PointerArray *selectedAi(const AiCollectionView *self) {

  const CollectionView *this = (const CollectionView *) self;

  PointerArray *selected = $(alloc(PointerArray), init);

  Array *selection = $(this, selectionIndexPaths);
  for (size_t i = 0; i < selection->count; i++) {
    const IndexPath *indexPath = $(selection, objectAtIndex, i);
    $(selected, add, this->dataSource.objectForItemAtIndexPath(this, indexPath));
  }

  release(selection);

  return selected;
}

/**
 * @fn void AiCollectionView::setNames(AiCollectionView *self, const char *names)
 * @memberof AiCollectionView
 */
static void setNames(AiCollectionView *self, const char *names) {

  self->numShown = 0;

  if (names) {
    char buffer[MAX_STRING_CHARS];
    Str_Copy(buffer, names, sizeof(buffer));

    for (const char *name = strtok(buffer, " \t\n"); name; name = strtok(NULL, " \t\n")) {
      for (size_t i = 0; i < self->numAi; i++) {
        if (!Str_CaseCompare(self->ai[i].name, name) && self->numShown < self->numAi) {
          self->shown[self->numShown++] = i;
          break;
        }
      }
    }
  } else {
    for (size_t i = 0; i < self->numAi; i++) {
      self->shown[self->numShown++] = i;
    }
  }

  $((CollectionView *) self, reloadData);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;

  ((AiCollectionViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
  ((AiCollectionViewInterface *) clazz->interface)->selectedAi = selectedAi;
  ((AiCollectionViewInterface *) clazz->interface)->setNames = setNames;
}

/**
 * @fn Class *AiCollectionView::_AiCollectionView(void)
 * @memberof AiCollectionView
 */
Class *_AiCollectionView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "AiCollectionView",
      .superclass = _CollectionView(),
      .instanceSize = sizeof(AiCollectionView),
      .interfaceSize = sizeof(AiCollectionViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
