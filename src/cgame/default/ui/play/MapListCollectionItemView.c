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

#include "MapListCollectionItemView.h"

#define _Class _MapListCollectionItemView

#pragma mark - MapListCollectionItemView

/**
 * @fn MapListCollectionItemView *MapListCollectionItemView::initWithFrame(MapListCollectionItemView *self, const SDL_Rect *frame)
 *
 * @memberof MapListCollectionItemView
 */
static MapListCollectionItemView *initWithFrame(MapListCollectionItemView *self, const SDL_Rect *frame) {

	self = (MapListCollectionItemView *) super(CollectionItemView, self, initWithFrame, frame);
	if (self) {
		self->collectionItemView.text->view.alignment = ViewAlignmentBottomCenter;
	}

	return self;
}

/**
 * @fn void MapListCollectionItemView::setMapListItemInfo(MapListCollectionItemView *self, MapListItemInfo *info);
 *
 * @memberof MapListCollectionItemView
 */
static void setMapListItemInfo(MapListCollectionItemView *self, MapListItemInfo *info) {

	CollectionItemView *item = (CollectionItemView *) self;

	$(item->text, setText, NULL);
	$(item->imageView, setImage, NULL);

	if (info) {
		$(item->text, setText, info->message);
		$(item->imageView, setImageWithSurface, info->mapshot);
	}
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((MapListCollectionItemViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
	((MapListCollectionItemViewInterface *) clazz->interface)->setMapListItemInfo = setMapListItemInfo;
}

/**
 * @fn Class *MapListCollectionItemView::_MapListCollectionItemView(void)
 * @memberof MapListCollectionItemView
 */
Class *_MapListCollectionItemView(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "MapListCollectionItemView",
			.superclass = _CollectionItemView(),
			.instanceSize = sizeof(MapListCollectionItemView),
			.interfaceOffset = offsetof(MapListCollectionItemView, interface),
			.interfaceSize = sizeof(MapListCollectionItemViewInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class

