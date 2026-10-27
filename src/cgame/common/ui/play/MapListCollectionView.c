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

#include "MapListCollectionView.h"
#include "MapListCollectionItemView.h"

#define _Class _MapListCollectionView

#define DEFAULT_GAMES "dm"

#pragma mark CollectionViewDataSource

/**
 * @see CollectionViewDataSource::numberOfItems(const CollectionView *)
 */
static size_t numberOfItems(const CollectionView *collectionView) {

  const MapListCollectionView *this = (const MapListCollectionView *) collectionView;

  return $(this->maps, count);
}

/**
 * @see CollectionViewDataSource::objectForItemAtIndexPath(const CollectionView *, const IndexPath *)
 */
static ident objectForItemAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

  const MapListCollectionView *this = (const MapListCollectionView *) collectionView;

  const size_t index = $(indexPath, indexAtPosition, 0);

  return $(this->maps, get, index);
}

#pragma mark - CollectionViewDelegate

/**
 * @see CollectionViewDelegate::itemForObjectAtIndex(const CollectionView *, const IndexPath *)
 */
static CollectionItemView *itemForObjectAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

  const MapListCollectionView *this = (const MapListCollectionView *) collectionView;
  const size_t index = $(indexPath, indexAtPosition, 0);

  const MapListItemInfo *info = $(this->maps, get, index);

  MapListCollectionItemView *item = $(alloc(MapListCollectionItemView), initWithFrame, NULL);
  assert(item);

  $(item, setMapListItemInfo, info);

  return (CollectionItemView *) item;
}

#pragma mark - Asynchronous map loading

/**
 * @brief Loads the information of the map at `path` from its BSP: its title, from the worldspawn
 * `message`, and the games it is made for.
 * @return True if the map loaded.
 */
bool Cg_LoadMapListItemInfo(const char *path, MapListItemInfo *info) {

  memset(info, 0, sizeof(*info));

  bool loaded = false;

  File *file = cgi.OpenFile(path);
  if (file) {

    BspHeader header;
    if (cgi.ReadFile(file, (void *) &header, sizeof(header), 1) == 1) {

      for (size_t i = 0; i < sizeof(header) / sizeof(int32_t); i++) {
        ((int32_t *) &header)[i] = LittleLong(((int32_t *) &header)[i]);
      }

      if (header.version != BSP_VERSION) {
        Cg_Warn("Invalid BSP header found in %s: %d\n", path, header.version);
        cgi.CloseFile(file);
        return false;
      }

      Str_Copy(info->mapname, path, sizeof(info->mapname));
      Str_Copy(info->message, path, sizeof(info->message));
      Str_Copy(info->games, DEFAULT_GAMES, sizeof(info->games));

      const int32_t size = header.lumps[BSP_LUMP_ENTITIES].fileLen;

      char *entities = malloc(size + 1);
      entities[size] = '\0';

      cgi.SeekFile(file, header.lumps[BSP_LUMP_ENTITIES].fileOfs);
      cgi.ReadFile(file, entities, 1, size);

      Parser parser = Parse_Init(entities, PARSER_NO_COMMENTS);
      char key[MAX_BSP_ENTITY_KEY], token[MAX_BSP_ENTITY_VALUE];

      while (true) {

        if (!Parse_Token(&parser, PARSE_DEFAULT, key, sizeof(key))) {
          break;
        }

        if (Str_Compare(key, "}") == 0) {
          break;
        }

        if (Str_Compare(key, "{") == 0) {
          continue;
        }

        if (!Parse_Token(&parser, PARSE_DEFAULT | PARSE_ALLOW_OVERRUN, token, sizeof(token))) {
          break;
        }

        if (Str_Compare(key, "games") == 0) {
          Str_Copy(info->games, token, sizeof(info->games));
        } else if (Str_Compare(key, "message") == 0) {
          Str_StripColors(token, info->message);

          char *c = Str_Find(info->message, "\\n");
          if (c) {
            *c = '\0';
          }

          c = Str_Find(info->message, " - ");
          if (c) {
            *c = '\0';
          }

          c = Str_Find(info->message, " by ");
          if (c) {
            *c = '\0';
          }
        }
      }

      free(entities);

      loaded = true;
    }

    cgi.CloseFile(file);
  }

  return loaded;
}


/**
 * @brief FsEnumerator for map discovery.
 */
static void enumerateMaps(const char *path, void *data) {

  const Thread *thread = $$(Thread, currentThread);
  if (thread && $(thread, isCancelled)) {
    return;
  }

  MapList *maps = data;

  MapListItemInfo *info = calloc(1, sizeof(*info));
  assert(info);

  if (Cg_LoadMapListItemInfo(path, info) && GameInfo_HasMap(&cgameInfo, info->games)) {
    $(maps, add, info);
  } else {
    free(info);
  }
}

/**
 * @brief `ThreadFunction` for asynchronous map info loading.
 */
static ident loadMaps(Thread *thread) {

  MapList *maps = thread->data;

  cgi.EnumerateFiles("maps/*.bsp", enumerateMaps, maps);

  release(maps);

  return NULL;
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  MapListCollectionView *this = (MapListCollectionView *) self;

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

  const Array *items = (Array *) this->collectionView.items;
  if ($(this->maps, count) != items->count) {
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
    self->maps = $(alloc(MapList), init);
    assert(self->maps);

    $(cgi.threadPool, detach, loadMaps, retain(self->maps));

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
