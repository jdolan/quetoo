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

#include "DemosCollectionView.h"

#define _Class _DemosCollectionView

/**
 * @brief Mapshots are scaled once, on the loading thread, to the item size `item-size` in
 * DemosViewController.css gives each item, so that every demo tile is the same shape.
 */
#define DEMO_MAPSHOT_WIDTH  448
#define DEMO_MAPSHOT_HEIGHT 252

#pragma mark CollectionViewDataSource

/**
 * @see CollectionViewDataSource::numberOfItems(const CollectionView *)
 */
static size_t numberOfItems(const CollectionView *collectionView) {

  const DemosCollectionView *this = (const DemosCollectionView *) collectionView;

  return $(this->demos, count);
}

/**
 * @see CollectionViewDataSource::objectForItemAtIndexPath(const CollectionView *, const IndexPath *)
 */
static ident objectForItemAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

  const DemosCollectionView *this = (const DemosCollectionView *) collectionView;

  const size_t index = $(indexPath, indexAtPosition, 0);

  return $(this->demos, get, index);
}

#pragma mark - CollectionViewDelegate

/**
 * @see CollectionViewDelegate::itemForObjectAtIndex(const CollectionView *, const IndexPath *)
 */
static CollectionItemView *itemForObjectAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

  DemosCollectionView *this = (DemosCollectionView *) collectionView;
  const size_t index = $(indexPath, indexAtPosition, 0);

  DemoListItemInfo *info = $(this->demos, get, index);

  DemosCollectionItemView *item = $(alloc(DemosCollectionItemView), initWithFrame, NULL);
  assert(item);

  item->collectionView = this;
  $(item, setDemoListItemInfo, info);

  return (CollectionItemView *) item;
}

#pragma mark - Asynchronous demo loading

/**
 * @brief Fs_Enumerator for demo discovery.
 */
static void enumerateDemos(const char *path, void *data) {

  DemoList *demos = data;

  file_t *file = cgi.OpenFile(path);
  if (!file) {
    return;
  }

  demo_header_t header;
  if (cgi.ReadFile(file, &header, sizeof(header), 1) != 1 ||
      memcmp(header.magic, DEMO_MAGIC, sizeof(header.magic)) ||
      LittleLong(header.version) != DEMO_VERSION) {
    cgi.CloseFile(file);
    return;
  }

  header.duration = LittleLong(header.duration);
  header.favorite = LittleLong(header.favorite);

  // these are read verbatim from disk with no guarantee of NUL-termination; a corrupt or
  // malicious file that fills a whole field could otherwise send q_strlcpy's strlen scanning
  // past it into whatever follows on the stack
  header.map[sizeof(header.map) - 1] = '\0';
  header.message[sizeof(header.message) - 1] = '\0';
  header.title[sizeof(header.title) - 1] = '\0';

  DemoListItemInfo *info = calloc(1, sizeof(*info));

  q_strlcpy(info->filename, path, sizeof(info->filename));
  q_strlcpy(info->map, header.map, sizeof(info->map));
  q_strlcpy(info->message, header.message, sizeof(info->message));
  q_strlcpy(info->title, header.title, sizeof(info->title));
  info->duration = header.duration;
  info->favorite = header.favorite != 0;

  fs_stat_t stat;
  if (cgi.StatFile(path, &stat)) {
    info->modified = stat.modified;
  }

  cgi.CloseFile(file);

  List *mapshots = cgi.Mapshots(info->map);

  const uint32_t len = mapshots ? (uint32_t) mapshots->count : 0;
  if (len) {
    const ListNode *node = mapshots->head;
    for (size_t i = 0, index = RandomRangeu(0, len); node && i < index; i++) {
      node = node->next;
    }

    SDL_Surface *surf = node ? cgi.LoadSurface(node->element) : NULL;
    if (surf) {
      info->mapshot = SDL_CreateSurface(DEMO_MAPSHOT_WIDTH, DEMO_MAPSHOT_HEIGHT, SDL_PIXELFORMAT_RGB24);
      SDL_BlitSurfaceScaled(surf, NULL, info->mapshot, NULL, SDL_SCALEMODE_LINEAR);
      SDL_DestroySurface(surf);
    }
  }

  release(mapshots);

  $(demos, add, info);
}

/**
 * @brief ThreadRunFunc for asynchronous demo info loading.
 */
static void loadDemos(void *data) {

  DemoList *demos = data;

  cgi.EnumerateFiles("demos/*.demo", enumerateDemos, demos);

  release(demos);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  DemosCollectionView *this = (DemosCollectionView *) self;

  release(this->demos);

  super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((DemosCollectionView *) self, initWithFrame, NULL);
}

/**
 * @see View::layoutIfNeeded(View *)
 */
static void layoutIfNeeded(View *self) {

  DemosCollectionView *this = (DemosCollectionView *) self;

  const Array *items = (Array *) this->collectionView.items;
  const size_t count = $(this->demos, count);

  if (count != items->count) {
    $((CollectionView *) this, reloadData);
  }

  const bool empty = count == 0;
  $(self, setVisibility, empty ? ViewVisibilityHidden : ViewVisibilityVisible);
  if (this->emptyStateView) {
    $(this->emptyStateView, setVisibility, empty ? ViewVisibilityVisible : ViewVisibilityHidden);
  }

  super(View, self, layoutIfNeeded);
}

#pragma mark - DemosCollectionView

/**
 * @fn DemosCollectionView *DemosCollectionView::initWithFrame(DemosCollectionView *self, const SDL_Rect *frame)
 * @memberof DemosCollectionView
 */
static DemosCollectionView *initWithFrame(DemosCollectionView *self, const SDL_Rect *frame) {

  self = (DemosCollectionView *) super(CollectionView, self, initWithFrame, frame);
  if (self) {
    self->demos = $(alloc(DemoList), init);
    assert(self->demos);

    self->collectionView.dataSource.numberOfItems = numberOfItems;
    self->collectionView.dataSource.objectForItemAtIndexPath = objectForItemAtIndexPath;
    self->collectionView.delegate.itemForObjectAtIndexPath = itemForObjectAtIndexPath;

    $(self, reloadDemos);
  }

  return self;
}

/**
 * @fn void DemosCollectionView::reloadDemos(DemosCollectionView *self)
 * @memberof DemosCollectionView
 */
static void reloadDemos(DemosCollectionView *self) {
  cgi.Thread(__func__, loadDemos, retain(self->demos), THREAD_NO_WAIT);
}

/**
 * @fn void DemosCollectionView::setFilter(DemosCollectionView *self, const char *filter)
 * @memberof DemosCollectionView
 */
static void setFilter(DemosCollectionView *self, const char *filter) {

  $(self->demos, setFilter, filter);

  $((CollectionView *) self, reloadData);
}

/**
 * @fn DemoListItemInfo *DemosCollectionView::selectedDemo(const DemosCollectionView *self)
 * @memberof DemosCollectionView
 */
static DemoListItemInfo *selectedDemo(const DemosCollectionView *self) {

  const CollectionView *this = (const CollectionView *) self;

  Array *selection = $(this, selectionIndexPaths);

  DemoListItemInfo *info = NULL;
  if (selection->count) {
    const IndexPath *indexPath = $(selection, objectAtIndex, 0);
    info = this->dataSource.objectForItemAtIndexPath(this, indexPath);
  }

  release(selection);

  return info;
}

/**
 * @fn void DemosCollectionView::removeDemo(DemosCollectionView *self, const char *filename)
 * @memberof DemosCollectionView
 */
static void removeDemo(DemosCollectionView *self, const char *filename) {

  $(self->demos, remove, filename);

  $((CollectionView *) self, reloadData);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->layoutIfNeeded = layoutIfNeeded;

  ((DemosCollectionViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
  ((DemosCollectionViewInterface *) clazz->interface)->reloadDemos = reloadDemos;
  ((DemosCollectionViewInterface *) clazz->interface)->setFilter = setFilter;
  ((DemosCollectionViewInterface *) clazz->interface)->selectedDemo = selectedDemo;
  ((DemosCollectionViewInterface *) clazz->interface)->removeDemo = removeDemo;
}

/**
 * @fn Class *DemosCollectionView::_DemosCollectionView(void)
 * @memberof DemosCollectionView
 */
Class *_DemosCollectionView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "DemosCollectionView",
      .superclass = _CollectionView(),
      .instanceSize = sizeof(DemosCollectionView),
      .interfaceSize = sizeof(DemosCollectionViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
