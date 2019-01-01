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

#include <Objectively/Value.h>

#include "cg_local.h"

#include "MapListCollectionView.h"
#include "MapListCollectionItemView.h"

#include "parse.h"

#define _Class _MapListCollectionView

#pragma mark CollectionViewDataSource

/**
 * @see CollectionViewDataSource::numberOfItems(const CollectionView *)
 */
static size_t numberOfItems(const CollectionView *collectionView) {

	const MapListCollectionView *this = (MapListCollectionView *) collectionView;

	return ((Array *) this->maps)->count;
}

/**
 * @see CollectionViewDataSource::objectForItemAtIndexPath(const CollectionView *, const IndexPath *)
 */
static ident objectForItemAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

	const MapListCollectionView *this = (MapListCollectionView *) collectionView;

	const size_t index = $(indexPath, indexAtPosition, 0);

	return $((Array *) this->maps, objectAtIndex, index);
}

#pragma mark - CollectionViewDelegate

/**
 * @see CollectionViewDelegate::itemForObjectAtIndex(const CollectionView *, const IndexPath *)
 */
static CollectionItemView *itemForObjectAtIndexPath(const CollectionView *collectionView, const IndexPath *indexPath) {

	const MapListCollectionView *this = (MapListCollectionView *) collectionView;
	const size_t index = $(indexPath, indexAtPosition, 0);

	Value *value = $((Array *) this->maps, objectAtIndex, index);

	MapListCollectionItemView *item = $(alloc(MapListCollectionItemView), initWithFrame, NULL);
	assert(item);

	$(item, setMapListItemInfo, value->value);

	return (CollectionItemView *) item;
}

#pragma mark - Asynchronous map loading

/**
 * @brief Comparator for map sorting.
 */
static Order sortMaps(const ident a, const ident b) {

	const MapListItemInfo *c = ((const Value *) a)->value;
	const MapListItemInfo *d = ((const Value *) b)->value;

	const char *e = g_str_has_prefix(c->message, "The ") ? c->message + 4 : c->message;
	const char *f = g_str_has_prefix(d->message, "The ") ? d->message + 4 : d->message;

	return g_ascii_strcasecmp(e, f) < 0 ? OrderAscending : OrderDescending;
}

/**
 * @brief Fs_Enumerator for map discovery.
 */
static void enumerateMaps(const char *path, void *data) {

	MapListCollectionView *this = (MapListCollectionView *) data;

	const Array *maps = (Array *) this->maps;
	for (size_t i = 0; i < maps->count; i++) {

		const Value *value = $(maps, objectAtIndex, i);
		const MapListItemInfo *info = value->value;

		if (g_strcmp0(info->mapname, path) == 0) {
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
				cgi.Warn("Invalid BSP header found in %s: %d\n", path, header.version);
				cgi.CloseFile(file);
				return;
			}

			MapListItemInfo *info = g_new0(MapListItemInfo, 1);

			g_strlcpy(info->mapname, path, sizeof(info->mapname));
			g_strlcpy(info->message, path, sizeof(info->message));

			gchar *entities = g_malloc(header.lumps[BSP_LUMP_ENTITIES].file_len);

			cgi.SeekFile(file, header.lumps[BSP_LUMP_ENTITIES].file_ofs);
			cgi.ReadFile(file, entities, 1, header.lumps[BSP_LUMP_ENTITIES].file_len);

			parser_t parser;
			char token[MAX_BSP_ENTITY_VALUE];

			Parse_Init(&parser, entities, PARSER_NO_COMMENTS);

			while (true) {
				
				if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
					break;
				}

				if (g_strcmp0(token, "message") == 0) {
					
					if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
						break;
					}

					StripColors(token, info->message);

					char *c = strstr(info->message, "\\n");
					if (c) {
						*c = '\0';
					}

					c = strstr(info->message, " - ");
					if (c) {
						*c = '\0';
					}

					c = strstr(info->message, " by ");
					if (c) {
						*c = '\0';
					}

					break;
				}
			}

			g_free(entities);

			GList *mapshots = cgi.Mapshots(path);

			const guint len = g_list_length(mapshots);
			if (len) {
				const char *mapshot = g_list_nth_data(mapshots, Randomr(0, len));

				SDL_Surface *surf;
				if (cgi.LoadSurface(mapshot, &surf)) {
					SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_NONE);
					info->mapshot = SDL_CreateRGBSurface(0,
					                                     this->collectionView.itemSize.w * 2,
					                                     this->collectionView.itemSize.h * 2,
					                                     surf->format->BitsPerPixel,
					                                     surf->format->Rmask,
					                                     surf->format->Gmask,
					                                     surf->format->Bmask,
					                                     surf->format->Amask
					                                    );
					SDL_SetSurfaceBlendMode(info->mapshot, SDL_BLENDMODE_NONE);
					SDL_BlitScaled(surf, NULL, info->mapshot, NULL);
				} else {
					info->mapshot = NULL;
				}
			}

			g_list_free_full(mapshots, g_free);

			Value *value = $(alloc(Value), initWithValue, info);

			synchronized(this->lock, {
				$(this->maps, addObject, value);
				$(this->maps, sort, sortMaps);
			});
		}

		cgi.CloseFile(file);
	}
}

/**
 * @brief ThreadRunFunc for asynchronous map info loading.
 */
static void loadMaps(void *data) {

	MapListCollectionView *this = data;

	cgi.EnumerateFiles("maps/*.bsp", enumerateMaps, this);
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

	synchronized(this->lock, {

		const Array *maps = (Array *) this->maps;
		const Array *items = (Array *) this->collectionView.items;

		if (maps->count != items->count) {
			$((CollectionView *) this, reloadData);
		}

		super(View, self, layoutIfNeeded);
	});
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

		self->maps = $$(MutableArray, array);
		assert(self->maps);

		cgi.Thread(__func__, loadMaps, self, THREAD_NO_WAIT);

		self->collectionView.dataSource.numberOfItems = numberOfItems;
		self->collectionView.dataSource.objectForItemAtIndexPath = objectForItemAtIndexPath;
		self->collectionView.delegate.itemForObjectAtIndexPath = itemForObjectAtIndexPath;

		self->collectionView.itemSize = MakeSize(240, 135);
	}

	return self;
}

/**
 * @fn Array *MapListCollectionView::selectedMaps(const MapListCollectionView *self)
 * @memberof MapListCollectionView
 */
static Array *selectedMaps(const MapListCollectionView *self) {

	CollectionView *this = (CollectionView *) self;

	MutableArray *selectedMaps = $$(MutableArray, array);

	Array *selection = $(this, selectionIndexPaths);
	for (size_t i = 0; i < selection->count; i++) {
		const IndexPath *indexPath = $(selection, objectAtIndex, i);

		Value *value = this->dataSource.objectForItemAtIndexPath(this, indexPath);
		$(selectedMaps, addObject, value);
	}

	release(selection);

	return (Array *) selectedMaps;
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
			.interfaceOffset = offsetof(MapListCollectionView, interface),
			.interfaceSize = sizeof(MapListCollectionViewInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
