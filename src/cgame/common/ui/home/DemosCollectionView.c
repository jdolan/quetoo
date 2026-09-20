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
 * @brief What one enumeration pass carries: the list it fills, and a count of what it left out.
 */
typedef struct {
  DemoList *demos;
  int32_t skipped;
} DemoEnumeration;

/**
 * @brief Reads one NUL-terminated string from an open demo, as `Net_WriteString` wrote it.
 * @return False if it does not end within `len`, which means the recording is not what it says.
 */
static bool readDemoString(File *file, char *out, size_t len) {

  for (size_t i = 0; i < len; i++) {
    if (cgi.ReadFile(file, out + i, 1, 1) != 1) {
      return false;
    }
    if (out[i] == '\0') {
      return true;
    }
  }

  return false;
}

/**
 * @brief Reads which protocol, and which client game, an open demo was recorded under.
 * @details Read from the stream rather than the header, because only the stream names the
 * module: each chunk is a size and a frame number, and the first message of any recording is
 * `SV_CMD_SERVER_DATA`, carrying the major, the minor, the demo flag, the game and the client
 * game. `Sv_ReadDemoStreamProtocol` reads the same bytes across the module boundary. A version
 * 3 header states the two protocols as well, which is what spares playback this seek.
 * @return False if the demo is too short or too strange to say, which is not the same as saying
 * something this build disagrees with.
 */
static bool readDemoProtocol(File *file, int32_t version, int32_t *major, int32_t *minor,
                             char *cgame, size_t len) {

  if (!cgi.SeekFile(file, (int64_t) DemoHeaderSize(version))) {
    return false;
  }

  int32_t size, frameNum, fields[2];
  byte cmd, demoServer;
  char game[MAX_QPATH];

  if (cgi.ReadFile(file, &size, sizeof(size), 1) != 1 ||
      cgi.ReadFile(file, &frameNum, sizeof(frameNum), 1) != 1 ||
      cgi.ReadFile(file, &cmd, sizeof(cmd), 1) != 1 ||
      cmd != SV_CMD_SERVER_DATA ||
      cgi.ReadFile(file, fields, sizeof(fields), 1) != 1 ||
      cgi.ReadFile(file, &demoServer, sizeof(demoServer), 1) != 1 ||
      !readDemoString(file, game, sizeof(game)) ||
      !readDemoString(file, cgame, len)) {
    return false;
  }

  *major = LittleLong(fields[0]);
  *minor = LittleLong(fields[1]);
  return true;
}

/**
 * @brief Fs_Enumerator for demo discovery.
 */
static void enumerateDemos(const char *path, void *data) {

  DemoEnumeration *enumeration = data;
  DemoList *demos = enumeration->demos;

  File *file = cgi.OpenFile(path);
  if (!file) {
    return;
  }

  // only what every version of the header has. The browser needs none of the fields later
  // versions added, and reading for them would fail on a demo recorded before they existed
  DemoHeader header;
  memset(&header, 0, sizeof(header));

  if (cgi.ReadFile(file, &header, DemoHeaderSize(DEMO_VERSION_MIN), 1) != 1 ||
      memcmp(header.magic, DEMO_MAGIC, sizeof(header.magic))) {
    cgi.CloseFile(file);
    return;
  }

  const int32_t version = LittleLong(header.version);

  if (version < DEMO_VERSION_MIN || version > DEMO_VERSION) {
    Cg_Debug("Skipping %s: demo version %d, this build reads %d through %d\n",
             path, version, DEMO_VERSION_MIN, DEMO_VERSION);
    enumeration->skipped++;
    cgi.CloseFile(file);
    return;
  }

  // listing a demo nothing can play only leads the player to a dead Play button, so leave it
  // out. A recording that cannot say which protocol it is gets the benefit of the doubt,
  // exactly as playback gives it
  int32_t major = 0, minor = 0;
  char cgame[MAX_QPATH];

  if (readDemoProtocol(file, version, &major, &minor, cgame, sizeof(cgame)) && major) {

    // the minor is the client game's, so it is only ours to judge when the recording names the
    // module we are running. Another module's demo plays under that module, which will have
    // its own answer, and hiding it here would hide something playable
    const bool ours = !q_strcmp(cgame, GAME_NAME);

    if (major != PROTOCOL_MAJOR || (ours && minor != PROTOCOL_MINOR)) {

      Cg_Debug("Skipping %s: %s protocol %d.%d, this is %d.%d\n",
               path, cgame, major, minor, PROTOCOL_MAJOR, PROTOCOL_MINOR);
      enumeration->skipped++;
      cgi.CloseFile(file);
      return;
    }
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

  FsStat stat;
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

  DemoEnumeration enumeration = { .demos = demos };

  cgi.EnumerateFiles("demos/*.demo", enumerateDemos, &enumeration);

  // one line, not one per file: this runs on every visit to the Demos screen, and twice on the
  // first, so naming each of them would bury whatever else is in the console
  if (enumeration.skipped) {
    Cg_Warn("Left out %d demo(s) this build cannot play; `debug cgame` names them\n",
            enumeration.skipped);
  }

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
