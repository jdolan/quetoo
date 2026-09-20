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

/**
 * @brief The mapshot surfaces are scaled once, on the loading thread, so they take a fixed
 * resolution rather than the item size, which the cascade owns and which that thread may not read.
 */
#define MAPSHOT_WIDTH  420
#define MAPSHOT_HEIGHT 236

#define DEFAULT_GAMES "dm"

#pragma mark - Cg_FilterCreateServerMapList

/**
 * @brief The tail of the `Cg_FilterCreateServerMapList` hook, listing a map made for this game.
 */
static bool Cg_FilterCreateServerMapList_Common(const MapListItemInfo *info) {
  return q_str_has_token(info->games, GAME_NAME);
}

FilterCreateServerMapList Cg_FilterCreateServerMapList = Cg_FilterCreateServerMapList_Common;

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
 * @brief Fs_Enumerator for map discovery.
 */
static void enumerateMaps(const char *path, void *data) {

  MapList *maps = data;

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
        return;
      }

      MapListItemInfo *info = calloc(1, sizeof(*info));

      q_strlcpy(info->mapname, path, sizeof(info->mapname));
      q_strlcpy(info->message, path, sizeof(info->message));
      q_strlcpy(info->games, DEFAULT_GAMES, sizeof(info->games));

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

        if (q_strcmp(key, "}") == 0) {
          break;
        }

        if (q_strcmp(key, "{") == 0) {
          continue;
        }

        if (!Parse_Token(&parser, PARSE_DEFAULT | PARSE_ALLOW_OVERRUN, token, sizeof(token))) {
          break;
        }

        if (q_strcmp(key, "games") == 0) {
          q_strlcpy(info->games, token, sizeof(info->games));
        } else if (q_strcmp(key, "message") == 0) {
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
        }
      }

      free(entities);

      if (!Cg_FilterCreateServerMapList(info)) {
        free(info);
        cgi.CloseFile(file);
        return;
      }

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

      $(maps, add, info);
    }

    cgi.CloseFile(file);
  }
}

/**
 * @brief ThreadRunFunc for asynchronous map info loading.
 */
static void loadMaps(void *data) {

  MapList *maps = data;

  cgi.EnumerateFiles("maps/*.bsp", enumerateMaps, maps);

  release(maps);
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

    cgi.Thread(__func__, loadMaps, retain(self->maps), THREAD_NO_WAIT);

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
