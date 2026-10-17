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

#include "MapshotThumbnailView.h"

#define _Class _MapshotThumbnailView

/**
 * @brief What a worker needs to load one map's mapshots: never the view, which must be freed on
 * the main thread, only the queue it fills.
 */
typedef struct {
  ImageQueue *queue;
  char map[MAX_QPATH];
  int32_t width, height;
} MapshotThumbnailJob;

/**
 * @brief `ThreadFunction` that loads and scales each mapshot of the job's map, starting from a
 * random one.
 */
static ident loadMapshots(Thread *thread) {

  MapshotThumbnailJob *job = thread->data;

  if ($(job->queue, isCancelled) || $(thread, isCancelled)) {
    release(job->queue);
    free(job);
    return NULL;
  }

  List *mapshots = cgi.Mapshots(job->map);

  const ListNode *node = mapshots->head;
  for (uint32_t i = RandomRangeu(0, (uint32_t) mapshots->count); node && i; i--) {
    node = node->next;
  }

  for (size_t i = 0; i < mapshots->count; i++) {

    if ($(job->queue, isCancelled) || $(thread, isCancelled)) {
      break;
    }

    SDL_Surface *surface = cgi.LoadSurface(node->element);
    if (surface) {
      SDL_Surface *scaled = SDL_ScaleSurface(surface, job->width, job->height,
                                             SDL_SCALEMODE_LINEAR);
      SDL_DestroySurface(surface);

      if (scaled) {
        Image *image = $$(Image, imageWithSurface, scaled);
        SDL_DestroySurface(scaled);

        if (image) {
          $(job->queue, add, image);
          release(image);
        }
      }
    } else {
      Cg_Warn("Failed to load %s\n", (const char *) node->element);
    }

    node = node->next ?: mapshots->head;
  }

  release(mapshots);
  release(job->queue);
  free(job);

  return NULL;
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  MapshotThumbnailView *this = (MapshotThumbnailView *) self;

  if (this->queue) {
    $(this->queue, cancel);
    release(this->queue);
  }

  super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((MapshotThumbnailView *) self, initWithFrame, NULL);
}

/**
 * @see View::render(View *, Renderer *)
 */
static void render(View *self, Renderer *renderer) {

  MapshotThumbnailView *this = (MapshotThumbnailView *) self;

  if (*this->map && this->queue == NULL) {
    const SDL_Rect bounds = $(self, bounds);
    const float density = self->window ? SDL_GetWindowPixelDensity(self->window) : 1.f;

    const int32_t width = (int32_t) (bounds.w * (density ?: 1.f));
    const int32_t height = (int32_t) (bounds.h * (density ?: 1.f));

    if (width > 0 && height > 0) {
      this->queue = $(alloc(ImageQueue), init);
      assert(this->queue);

      MapshotThumbnailJob *job = calloc(1, sizeof(*job));
      assert(job);

      job->queue = retain(this->queue);
      Str_Copy(job->map, this->map, sizeof(job->map));
      job->width = width;
      job->height = height;

      $(cgi.threadPool, detach, loadMapshots, job);
    }
  }

  if (this->queue) {
    Array *images = $(this->queue, drain);
    if (images) {
      for (size_t i = 0; i < images->count; i++) {
        Image *image = $(images, objectAtIndex, i);
        $(this->slideShow, addImage, image);
      }
      release(images);
    }
  }

  super(View, self, render, renderer);
}

#pragma mark - MapshotThumbnailView

/**
 * @brief Replaces the slideshow with an empty one, whose slide duration is its own.
 */
static void resetSlideShow(MapshotThumbnailView *self) {

  if (self->slideShow) {
    $((View *) self->slideShow, removeFromSuperview);
  }

  self->slideShow = $(alloc(SlideShowView), initWithFrame, NULL);
  assert(self->slideShow);

  self->slideShow->slideDuration = RandomRangeu(2000, 3501);

  $((View *) self, addSubview, (View *) self->slideShow);
  release(self->slideShow);
}

/**
 * @fn MapshotThumbnailView *MapshotThumbnailView::initWithFrame(MapshotThumbnailView *self, const SDL_Rect *frame)
 * @memberof MapshotThumbnailView
 */
static MapshotThumbnailView *initWithFrame(MapshotThumbnailView *self, const SDL_Rect *frame) {

  self = (MapshotThumbnailView *) super(View, self, initWithFrame, frame);
  if (self) {
    resetSlideShow(self);
  }

  return self;
}

/**
 * @fn void MapshotThumbnailView::setMap(MapshotThumbnailView *self, const char *map)
 * @memberof MapshotThumbnailView
 */
static void setMap(MapshotThumbnailView *self, const char *map) {

  if (!Str_Compare(self->map, map ?: "")) {
    return;
  }

  if (self->queue) {
    $(self->queue, cancel);
    self->queue = release(self->queue);
  }

  resetSlideShow(self);

  Str_Copy(self->map, map ?: "", sizeof(self->map));
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->render = render;

  ((MapshotThumbnailViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
  ((MapshotThumbnailViewInterface *) clazz->interface)->setMap = setMap;
}

/**
 * @fn Class *MapshotThumbnailView::_MapshotThumbnailView(void)
 * @memberof MapshotThumbnailView
 */
Class *_MapshotThumbnailView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "MapshotThumbnailView",
      .superclass = _View(),
      .instanceSize = sizeof(MapshotThumbnailView),
      .interfaceSize = sizeof(MapshotThumbnailViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
