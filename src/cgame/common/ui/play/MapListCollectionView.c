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

#include <Objectively/PointerArray.h>

#include "cg_local.h"

#include "MapListCollectionView.h"
#include "MapListCollectionItemView.h"

#define _Class _MapListCollectionView

/**
 * @brief The mapshot surfaces are scaled once, on the loading thread, so they take a fixed
 * resolution rather than the item size, which the cascade owns and which that thread may not read.
 */
#define MAPSHOT_WIDTH  420
#define MAPSHOT_HEIGHT 236

/**
 * @brief PointerArray destroy function for MapListItemInfo.
 */
static void freeMapListItemInfo(void *p) {
  MapListItemInfo *info = p;
  if (info->mapshot) {
    SDL_DestroySurface(info->mapshot);
  }
  free(info);
}

#pragma mark CollectionViewDataSource

/**
 * @see CollectionViewDataSource::numberOfItems(const CollectionView *)
 */
static size_t numberOfItems(const CollectionView *collectionView) {

  const MapListCollectionView *this = (const MapListCollectionView *) collectionView;

  // maps is rebuilt (added to, then sorted) under lock by the background loader; reading its
  // count without the same lock could observe it mid-sort
  size_t count;
  synchronized(this->lock, {
    count = this->maps->count;
  });

  return count;
}

/**
 * @see CollectionViewDataSource::objectForItemAtIndexPath(const CollectionView *, const IndexPath *)
 */
static ident objectForItemAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

  const MapListCollectionView *this = (const MapListCollectionView *) collectionView;

  const size_t index = $(indexPath, indexAtPosition, 0);

  ident object = NULL;
  synchronized(this->lock, {
    object = $(this->maps, get, index);
  });

  return object;
}

#pragma mark - CollectionViewDelegate

/**
 * @see CollectionViewDelegate::itemForObjectAtIndex(const CollectionView *, const IndexPath *)
 */
static CollectionItemView *itemForObjectAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

  const MapListCollectionView *this = (const MapListCollectionView *) collectionView;
  const size_t index = $(indexPath, indexAtPosition, 0);

  const MapListItemInfo *info;
  synchronized(this->lock, {
    info = $(this->maps, get, index);
  });

  MapListCollectionItemView *item = $(alloc(MapListCollectionItemView), initWithFrame, NULL);
  assert(item);

  $(item, setMapListItemInfo, info);

  return (CollectionItemView *) item;
}

#pragma mark - Asynchronous map loading

/**
 * @brief Hand-off to the worker thread: only the thread-safe state it needs, retained
 * independently of the View, so the View's own teardown never has to wait on or race the worker.
 */
typedef struct {
  Lock *lock;
  PointerArray *maps;
} MapListLoader;

/**
 * @brief Comparator for map sorting.
 */
static Order sortMaps(const ident a, const ident b) {

  const MapListItemInfo *c = a;
  const MapListItemInfo *d = b;

  const char *e = !q_strncmp(c->message, "The ", 4) ? c->message + 4 : c->message;
  const char *f = !q_strncmp(d->message, "The ", 4) ? d->message + 4 : d->message;

  return q_strcasecmp(e, f) < 0 ? OrderAscending : OrderDescending;
}

/**
 * @brief Fs_Enumerator for map discovery.
 */
static void enumerateMaps(const char *path, void *data) {

  const MapListLoader *loader = data;

  for (size_t i = 0; i < loader->maps->count; i++) {
    const MapListItemInfo *info = $(loader->maps, get, i);
    if (q_strcmp(info->mapname, path) == 0) {
      return;
    }
  }

  file_t *file = cgi.OpenFile(path);
  if (file) {

    bsp_header_t header;
    if (cgi.ReadFile(file, (void *) &header, sizeof(header), 1) == 1) {

      for (size_t i = 0; i < sizeof(header) / sizeof(int32_t); i++) {
        ((int32_t *) &header)[i] = LittleLong(((int32_t *) &header)[i]);
      }

      if (header.version != BSP_VERSION) {
        Cg_Warn("Invalid BSP header found in %s: %d\n", path, header.version);
        cgi.CloseFile(file);
        return;
      }

      MapListItemInfo *info = calloc(1, sizeof(*info));

      q_strlcpy(info->mapname, path, sizeof(info->mapname));
      q_strlcpy(info->message, path, sizeof(info->message));

      char *entities = malloc(header.lumps[BSP_LUMP_ENTITIES].file_len);

      cgi.SeekFile(file, header.lumps[BSP_LUMP_ENTITIES].file_ofs);
      cgi.ReadFile(file, entities, 1, header.lumps[BSP_LUMP_ENTITIES].file_len);

      parser_t parser = Parse_Init(entities, PARSER_NO_COMMENTS);;
      char token[MAX_BSP_ENTITY_VALUE];

      while (true) {
        
        if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
          break;
        }

        if (q_strcmp(token, "message") == 0) {
          
          if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
            break;
          }

          q_strcolorstrip(token, info->message);

          char *c = q_strstr(info->message, "\\n");
          if (c) {
            *c = '\0';
          }

          c = q_strstr(info->message, " - ");
          if (c) {
            *c = '\0';
          }

          c = q_strstr(info->message, " by ");
          if (c) {
            *c = '\0';
          }

          break;
        }
      }

      free(entities);

      List *mapshots = cgi.Mapshots(path);

      const uint32_t len = mapshots ? (uint32_t) mapshots->count : 0;
      if (len) {
        const size_t index = RandomRangeu(0, len);
        const ListNode *node = mapshots->head;
        for (size_t i = 0; node && i < index; i++) {
          node = node->next;
        }

        const char *mapshot = node ? node->element : NULL;

        SDL_Surface *surf = mapshot ? cgi.LoadSurface(mapshot) : NULL;
        if (surf) {
          info->mapshot = SDL_CreateSurface(MAPSHOT_WIDTH, MAPSHOT_HEIGHT, SDL_PIXELFORMAT_RGB24);
          SDL_BlitSurfaceScaled(surf, NULL, info->mapshot, NULL, SDL_SCALEMODE_LINEAR);
        } else {
          info->mapshot = NULL;
        }
      }

      release(mapshots);

      synchronized(loader->lock, {
        $(loader->maps, add, info);
        $(loader->maps, sort, sortMaps);
      });
    }

    cgi.CloseFile(file);
  }
}

/**
 * @brief ThreadRunFunc for asynchronous map info loading.
 */
static void loadMaps(void *data) {

  MapListLoader *loader = data;

  cgi.EnumerateFiles("maps/*.bsp", enumerateMaps, loader);

  release(loader->lock);
  release(loader->maps);

  free(loader);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  MapListCollectionView *this = (MapListCollectionView *) self;

  release(this->lock);
  release(this->maps);

  super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((MapListCollectionView *) self, initWithFrame, NULL);
}

/**
 * @see View::layoutIfNeeded(View *)
 */
static void layoutIfNeeded(View *self) {

  MapListCollectionView *this = (MapListCollectionView *) self;

  // reloadData must happen outside the lock: it synchronously calls back into numberOfItems/
  // objectForItemAtIndexPath, which themselves acquire this same (non-recursive) lock - holding
  // it across that call self-deadlocks the calling thread.
  bool needs_reload;
  synchronized(this->lock, {
    const Array *items = (Array *) this->collectionView.items;
    needs_reload = this->maps->count != items->count;
  });

  if (needs_reload) {
    $((CollectionView *) this, reloadData);
  }

  super(View, self, layoutIfNeeded);
}

#pragma mark - MapListCollectionView

/**
 * @fn MapListCollectionView *initWithFrame(MapListCollectionView *self, const SDL_Rect *frame)
 * @memberof MapListCollectionView
 */
static MapListCollectionView *initWithFrame(MapListCollectionView *self, const SDL_Rect *frame) {

  self = (MapListCollectionView *) super(CollectionView, self, initWithFrame, frame);
  if (self) {
    self->lock = $(alloc(Lock), init);
    assert(self->lock);

    self->maps = $(alloc(PointerArray), initWithDestroy, freeMapListItemInfo);
    assert(self->maps);

    MapListLoader *loader = malloc(sizeof(*loader));
    assert(loader);

    loader->lock = retain(self->lock);
    loader->maps = retain(self->maps);

    cgi.Thread(__func__, loadMaps, loader, THREAD_NO_WAIT);

    self->collectionView.dataSource.numberOfItems = numberOfItems;
    self->collectionView.dataSource.objectForItemAtIndexPath = objectForItemAtIndexPath;
    self->collectionView.delegate.itemForObjectAtIndexPath = itemForObjectAtIndexPath;
  }

  return self;
}

/**
 * @fn PointerArray *MapListCollectionView::selectedMaps(const MapListCollectionView *self)
 * @memberof MapListCollectionView
 */
static PointerArray *selectedMaps(const MapListCollectionView *self) {

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

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->layoutIfNeeded = layoutIfNeeded;

  ((MapListCollectionViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
  ((MapListCollectionViewInterface *) clazz->interface)->selectedMaps = selectedMaps;
}

/**
 * @fn Class *MapListCollectionView::_MapListCollectionView(void)
 * @memberof MapListCollectionView
 */
Class *_MapListCollectionView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "MapListCollectionView",
      .superclass = _CollectionView(),
      .instanceSize = sizeof(MapListCollectionView),
      .interfaceSize = sizeof(MapListCollectionViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
