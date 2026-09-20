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

#include "DemosCollectionItemView.h"
#include "DemosCollectionView.h"

#define _Class _DemosCollectionItemView

/**
 * @brief Resolves the name a demo is listed and filtered by: the user's own title if it has been
 * retitled, otherwise the map's human-readable name, falling back to the bsp's basename for a
 * demo recorded on a map that defines no worldspawn `message`.
 */
const char *DemoListItemName(const DemoListItemInfo *info) {

  if (*info->title) {
    return info->title;
  }

  if (*info->message) {
    return info->message;
  }

  char map[MAX_QPATH];
  StripExtension(Basename(info->map), map);

  return va("%s", map);
}

#pragma mark - Delegates

/**
 * @brief ButtonDelegate for the favorite (heart) button: toggles the demo's `favorite` header
 * field in place, without touching the rest of the file.
 */
static void didClickFavorite(Button *button) {

  DemosCollectionItemView *self = button->delegate.self;

  DemoListItemInfo *info = self->info;
  if (!info) {
    return;
  }

  const int32_t favorite = LittleLong(info->favorite ? 0 : 1);

  if (cgi.WriteFileAt(info->filename, &favorite, sizeof(favorite), offsetof(DemoHeader, favorite))) {
    info->favorite = !info->favorite;
    $((View *) self->collectionView, setNeedsLayout);
    $((DemosCollectionItemView *) self, setDemoListItemInfo, info);
  } else {
    Cg_Warn("Failed to update %s\n", info->filename);
  }
}

/**
 * @brief ButtonDelegate for the delete (trash) button.
 */
static void didClickDelete(Button *button) {

  DemosCollectionItemView *self = button->delegate.self;

  DemoListItemInfo *info = self->info;
  if (!info) {
    return;
  }

  char filename[MAX_QPATH];
  q_strlcpy(filename, info->filename, sizeof(filename));

  if (cgi.DeleteFile(filename)) {
    $(self->collectionView, removeDemo, filename);
  } else {
    Cg_Warn("Failed to delete %s\n", filename);
  }
}

/**
 * @brief TextViewDelegate for the title field: persists the edited name to the demo's `title`
 * header field in place. An empty title falls back to the map's own name at display time, so
 * clearing the field is how a demo is un-retitled.
 */
static void didEndEditingTitle(TextView *textView) {

  DemosCollectionItemView *self = textView->delegate.self;

  DemoListItemInfo *info = self->info;
  if (!info) {
    return;
  }

  // zeroed, not merely NUL-terminated: the whole fixed-size field is written back to the file,
  // so an uninitialized tail would put stack bytes on disk
  char title[MAX_QPATH] = { 0 };
  q_strlcpy(title, textView->attributedText->chars ?: "", sizeof(title));

  if (!q_strcmp(title, info->title)) {
    return;
  }

  if (cgi.WriteFileAt(info->filename, title, sizeof(title), offsetof(DemoHeader, title))) {
    q_strlcpy(info->title, title, sizeof(info->title));
  } else {
    Cg_Warn("Failed to update %s\n", info->filename);
  }

  // deliberately not reloading the collection here: this runs from the TextView's own
  // didEndEditing, and reloading destroys the item view and the TextView with it, leaving
  // TextView::stateDidChange to return into freed memory. A renamed demo may therefore stay
  // listed under a filter it no longer matches until the list is next reloaded
  $((DemosCollectionItemView *) self, setDemoListItemInfo, info);
}

/**
 * @brief An image Button for a demo's actions. Sizing and padding are styled by
 * `.demoItemActions Button` in ui/home/DemosViewController.css; nothing here assigns a styled
 * attribute, which the next theme pass would overwrite anyway.
 */
static Button *demoItemActionButton(const char *image, ButtonDelegate delegate) {

  Button *button = $(alloc(Button), initWithImage, Cg_LoadImage(image));
  assert(button);

  button->delegate = delegate;

  return button;
}

#pragma mark - DemosCollectionItemView

/**
 * @fn DemosCollectionItemView *DemosCollectionItemView::initWithFrame(DemosCollectionItemView *self, const SDL_Rect *frame)
 * @memberof DemosCollectionItemView
 */
static DemosCollectionItemView *initWithFrame(DemosCollectionItemView *self, const SDL_Rect *frame) {

  self = (DemosCollectionItemView *) super(CollectionItemView, self, initWithFrame, frame);
  if (self) {

    StackView *actions = $(alloc(StackView), initWithFrame, NULL);
    assert(actions);

    $((View *) actions, addClassName, "demoItemActions");

    self->favoriteButton = demoItemActionButton("pics/heart", (ButtonDelegate) {
      .self = self,
      .didClick = didClickFavorite
    });

    $((View *) actions, addSubview, (View *) self->favoriteButton);
    release(self->favoriteButton);

    self->deleteButton = demoItemActionButton("pics/trash", (ButtonDelegate) {
      .self = self,
      .didClick = didClickDelete
    });

    $((View *) actions, addSubview, (View *) self->deleteButton);
    release(self->deleteButton);

    $((View *) self, addSubview, (View *) actions);
    release(actions);

    self->titleView = $(alloc(TextView), initWithFrame, NULL);
    assert(self->titleView);

    $((View *) self->titleView, addClassName, "demoItemTitle");

    self->titleView->delegate.self = self;
    self->titleView->delegate.didEndEditing = didEndEditingTitle;

    $((View *) self, addSubview, (View *) self->titleView);
    release(self->titleView);
  }

  return self;
}

/**
 * @fn void DemosCollectionItemView::setDemoListItemInfo(DemosCollectionItemView *self, DemoListItemInfo *info)
 * @memberof DemosCollectionItemView
 */
static void setDemoListItemInfo(DemosCollectionItemView *self, DemoListItemInfo *info) {

  CollectionItemView *item = (CollectionItemView *) self;

  self->info = info;

  $(item->text, setText, NULL);
  $(item->imageView, setImage, NULL);

  if (info) {

    const int32_t seconds = info->duration / 1000;

    char date[32];
    const time_t modified = (time_t) info->modified;
    struct tm *tm = localtime(&modified);
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);

    $(item->text, setText, va("%s\n%d:%02d", date, seconds / 60, seconds % 60));

    if (info->mapshot) {
      $(item->imageView, setImageWithSurface, info->mapshot);
    }
  }

  $(self->titleView, setDefaultText, info ? DemoListItemName(info) : NULL);
  $(self->titleView, setAttributedText, info && *info->title ? info->title : NULL);

  $((View *) self->titleView, setVisibility, info ? ViewVisibilityVisible : ViewVisibilityHidden);
  $((View *) self->favoriteButton, setVisibility, info ? ViewVisibilityVisible : ViewVisibilityHidden);
  $((View *) self->deleteButton, setVisibility, info ? ViewVisibilityVisible : ViewVisibilityHidden);

  if (info) {
    self->favoriteButton->image->color = info->favorite ? Colors.Red : Colors.White;
  }
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((DemosCollectionItemViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
  ((DemosCollectionItemViewInterface *) clazz->interface)->setDemoListItemInfo = setDemoListItemInfo;
}

/**
 * @fn Class *DemosCollectionItemView::_DemosCollectionItemView(void)
 * @memberof DemosCollectionItemView
 */
Class *_DemosCollectionItemView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "DemosCollectionItemView",
      .superclass = _CollectionItemView(),
      .instanceSize = sizeof(DemosCollectionItemView),
      .interfaceSize = sizeof(DemosCollectionItemViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
