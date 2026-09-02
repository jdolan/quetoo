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

#include "JoinServerViewController.h"
#include "CvarCheckbox.h"
#include "CvarSlider.h"

static const char *_server = "Server";
static const char *_map = "Map";
static const char *_players = "Players";
static const char *_ping = "Ping";

/**
 * @brief Stands in for a detail field the selected server did not report.
 */
static const char *_unset = "—";

/**
 * @brief A ping the client never got an answer for.
 * @details `Cl_ParseServerInfo` clamps the measured round trip to 999, so 999
 * is the timeout sentinel rather than a latency. Reading it as "slow but
 * joinable" is the wrong story, so it renders unset and sorts last.
 */
#define JOIN_PING_UNANSWERED 999

static cvar_t *cg_join_server_hide_empty;
static cvar_t *cg_join_server_hide_bots;
static JoinServerViewController *sortingJoinServerViewController;

#define _Class _JoinServerViewController

static const cl_server_info_t *serverAtIndex(const PointerArray *servers, size_t index) {

  if (servers == NULL || index >= servers->count) {
    return NULL;
  }

  return $(servers, get, index);
}

/**
 * @brief Returns the selected server, by hostname rather than by row index,
 * so that the selection survives a re-sort and a refresh.
 */
static const cl_server_info_t *selectedServer(const JoinServerViewController *self) {

  if (self->servers == NULL || *self->selectedHostname == '\0') {
    return NULL;
  }

  for (size_t i = 0; i < self->servers->count; i++) {
    const cl_server_info_t *server = $(self->servers, get, i);
    if (q_strcmp(server->hostname, self->selectedHostname) == 0) {
      return server;
    }
  }

  return NULL;
}

static void setLabelText(Label *label, const char *text) {
  $(label->text, setText, text && *text ? text : NULL);
}

/**
 * @brief The colour threshold and the quick join threshold are the same
 * number, so that moving the slider visibly splits the list.
 */
static int32_t maxPing(void) {
  return Clampf(cg_quick_join_max_ping->integer, 1, 999);
}

/**
 * @brief True when the client never got an answer from this server.
 */
static bool pingUnanswered(const cl_server_info_t *server) {
  return server->ping <= 0 || server->ping >= JOIN_PING_UNANSWERED;
}

/**
 * @brief Formats a server address for display.
 * @details `Net_NetaddrToString` lives in the engine's socket layer, which
 * the modules do not link, so the dotted quad is read straight out of the
 * network byte order the address is stored in - no host byte order
 * assumption.
 */
static const char *addressLabel(const net_addr_t *addr) {

  const uint8_t *ip = (const uint8_t *) &addr->addr;
  const uint8_t *port = (const uint8_t *) &addr->port;

  return va("%u.%u.%u.%u:%u", ip[0], ip[1], ip[2], ip[3],
            (uint32_t) port[0] << 8 | port[1]);
}

static const char *sourceLabel(const cl_server_info_t *server) {

  switch (server->source) {
    case SERVER_SOURCE_INTERNET:
      return "Internet";
    case SERVER_SOURCE_USER:
      return "User";
    case SERVER_SOURCE_BCAST:
      return "LAN";
  }

  return _unset;
}

#pragma mark - Details pane

/**
 * @brief Shows or hides everything in the details pane that only means
 * something once a server is selected.
 */
static void setDetailsPopulated(JoinServerViewController *self, const bool populated) {

  $((View *) self->hintLabel, setHidden, populated);
  $(self->detailGrid, setHidden, !populated);
}

/**
 * @brief Shows the selected server's mapshot, when its map is installed locally.
 */
static void refreshMapshot(JoinServerViewController *self, const cl_server_info_t *server) {

  SDL_Surface *surface = NULL;

  if (server && *server->name) {
    List *mapshots = cgi.Mapshots(server->name);

    if (mapshots->count) {
      surface = cgi.LoadSurface(mapshots->head->element);
    }

    release(mapshots);
  }

  $(self->mapshotView, setImageWithSurface, surface);
  $((View *) self->mapshotView, setHidden, surface == NULL);

  if (surface) {
    SDL_DestroySurface(surface);
  }
}

/**
 * @brief Repopulates the details pane from the current selection.
 */
static void refreshDetails(JoinServerViewController *self) {

  const cl_server_info_t *server = selectedServer(self);

  setDetailsPopulated(self, server != NULL);
  refreshMapshot(self, server);

  if (server == NULL) {
    setLabelText(self->hostnameLabel, "Select a server");
    setLabelText(self->addressLabel, NULL);
    $((View *) self->connectButton, setHidden, true);
    return;
  }

  setLabelText(self->hostnameLabel, *server->hostname ? server->hostname : _unset);
  setLabelText(self->addressLabel, addressLabel(&server->addr));

  setLabelText(self->sourceLabel, sourceLabel(server));
  setLabelText(self->mapLabel, *server->name ? server->name : _unset);
  setLabelText(self->gameplayLabel, *server->gameplay ? server->gameplay : _unset);
  setLabelText(self->movementLabel, *server->movement ? server->movement : _unset);
  setLabelText(self->playersLabel, va("%d / %d", server->clients, server->max_clients));
  setLabelText(self->pingLabel, pingUnanswered(server) ? _unset : va("%d ms", server->ping));

  $((View *) self->connectButton, setHidden, false);
}

/**
 * @brief Restores the selection after a sort or a refresh.
 * @details The selection is held by hostname, so a re-sort keeps the same
 * server rather than the same row. When the selected server has gone from the
 * list entirely, the details pane empties rather than jumping to another one.
 */
static void restoreSelection(JoinServerViewController *self) {

  TableView *tableView = self->serversTableView;

  ssize_t index = -1;
  const size_t count = self->servers ? self->servers->count : 0;
  for (size_t row = 0; row < count; row++) {
    const cl_server_info_t *server = $(self->servers, get, row);
    if (q_strcmp(server->hostname, self->selectedHostname) == 0) {
      index = (ssize_t) row;
      break;
    }
  }

  if (index < 0) {
    self->selectedHostname[0] = '\0';
  }

  $(tableView, deselectAll);

  if (index >= 0) {
    $(tableView, selectRowAtIndex, (size_t) index);
  }

  refreshDetails(self);
}

#pragma mark - Delegates

static void didToggleHideBots(Checkbox *checkbox) {

  cvarCheckboxDidToggle(checkbox);

  $((JoinServerViewController *) checkbox->delegate.self, reloadServers);
}

static void didToggleHideEmpty(Checkbox *checkbox) {

  cvarCheckboxDidToggle(checkbox);

  $((JoinServerViewController *) checkbox->delegate.self, reloadServers);
}

/**
 * @brief SliderDelegate for the max ping slider.
 * @details CvarSlider writes the cvar in its own `setValue`; the delegate
 * slot is free, and is what repaints the ping column against the new
 * threshold.
 */
static void didSetMaxPing(Slider *slider, double value) {

  (void) value;

  JoinServerViewController *this = slider->delegate.self;

  $(this->serversTableView, reloadData);
}

/**
 * @brief ButtonDelegate for Quick Join.
 * @description Selects a server based on minumum ping and maximum players
 * with a bit of lovely random thrown in. Any server that matches the
 * criteria will be weighted by how much "better" they are by how much lower
 * their ping is and how many more players there are.
 */
static void didClickQuickJoin(Button *button) {

  JoinServerViewController *this = button->delegate.self;

  const int32_t max_ping = maxPing();
  const int32_t min_clients = Clampf(cg_quick_join_min_clients->integer, 0, MAX_CLIENTS);

  uint32_t total_weight = 0;

  const size_t count = this->servers ? this->servers->count : 0;

  for (size_t i = 0; i < count; i++) {
    const cl_server_info_t *server = $(this->servers, get, i);

    int32_t weight = 1;

    if (!(server->clients < min_clients || server->clients >= server->max_clients)) {
      // more weight for more populated servers
      weight += (server->clients - min_clients) * 5;

      // more weight for lower ping servers
      weight += (max_ping - server->ping) / 10;

      if (server->ping > max_ping) { // one third weight for high ping servers
        weight /= 3;
      }
    }

    total_weight += max(weight, 1);
  }

  if (total_weight == 0) {
    return;
  }

  const uint32_t random_weight = RandomRangeu(0, total_weight);
  uint32_t current_weight = 0;

  for (size_t i = 0; i < count; i++) {
    const cl_server_info_t *server = $(this->servers, get, i);

    int32_t weight = 1;

    if (server->ping > max_ping ||
      server->clients < min_clients ||
      server->clients >= server->max_clients) {

      weight = 0;
    } else {
      // more weight for more populated servers
      weight += server->clients - min_clients;

      // more weight for lower ping servers
      weight += (max_ping - server->ping) / 20;
    }

    current_weight += weight;

    if (current_weight > random_weight) {
      cgi.Connect(&server->addr);
      break;
    }
  }
}

/**
 * @brief ButtonDelegate for the Refresh button.
 */
static void didClickRefresh(Button *button) {
  cgi.GetServers();
}

/**
 * @brief ButtonDelegate for the Connect button.
 * @details Connect acts on the details pane's server, which is the selection
 * held by hostname - not on whatever row happens to be at the selected index.
 */
static void didClickConnect(Button *button) {

  JoinServerViewController *this = button->delegate.self;

  const cl_server_info_t *server = selectedServer(this);
  if (server) {
    cgi.Connect(&server->addr);
  }
}

#pragma mark - TableViewDataSource

static size_t numberOfRows(const TableView *tableView) {

  const JoinServerViewController *this = tableView->dataSource.self;

  return this->servers ? this->servers->count : 0;
}

#pragma mark - TableViewDelegate

static TableCellView *cellForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

  const JoinServerViewController *this = tableView->dataSource.self;

  cl_server_info_t *server = (cl_server_info_t *) serverAtIndex(this->servers, row);
  assert(server);

  TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);

  if (q_strlen(server->error)) {
    if (q_strcmp(column->identifier, _server) == 0) {
      $(cell->text, setText, server->error);
      $((View *) cell, addClassName, "error");
    }
    return cell;
  }

  if (q_strcmp(column->identifier, _server) == 0) {
    $(cell->text, setText, server->hostname);
  } else if (q_strcmp(column->identifier, _map) == 0) {
    $(cell->text, setText, server->name);
  } else if (q_strcmp(column->identifier, _players) == 0) {
    $(cell->text, setText, va("%d / %d", server->clients, server->max_clients));
  } else if (q_strcmp(column->identifier, _ping) == 0) {

    if (pingUnanswered(server)) {
      $(cell->text, setText, _unset);
      $((View *) cell, addClassName, "pingUnanswered");
    } else {
      $(cell->text, setText, va("%d ms", server->ping));

      const int32_t threshold = maxPing();
      if (server->ping > threshold) {
        $((View *) cell, addClassName, "pingOver");
      } else if (server->ping <= threshold / 2) {
        $((View *) cell, addClassName, "pingFast");
      }
    }
  }

  return cell;
}

static void didSetSortColumn(TableView *tableView) {
  $((JoinServerViewController *) tableView->delegate.self, reloadServers);
}

static void didSelectRowsAtIndexes(TableView *tableView, const IndexSet *indexes) {

  JoinServerViewController *this = tableView->delegate.self;

  if (indexes->count == 0) {
    return;
  }

  const cl_server_info_t *server = serverAtIndex(this->servers, indexes->indexes[0]);
  if (server == NULL) {
    return;
  }

  q_strlcpy(this->selectedHostname, server->hostname, sizeof(this->selectedHostname));
  refreshDetails(this);

  const SDL_PropertiesID props = SDL_GetWindowProperties(((View *) tableView)->window);
  const SDL_Event *event = SDL_GetPointerProperty(props, "event", NULL);
  if (event && event->button.clicks == 2) {
    cgi.Connect(&server->addr);
  }
}

#pragma mark - Object

static void dealloc(Object *self) {

  JoinServerViewController *this = (JoinServerViewController *) self;

  release(this->servers);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  JoinServerViewController *this = (JoinServerViewController *) self;

  Checkbox *hideEmpty, *hideBots;
  Button *refresh, *quickJoin;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("servers", &this->serversTableView),
    MakeOutlet("servers_empty", &this->emptyLabel),
    MakeOutlet("server_hostname", &this->hostnameLabel),
    MakeOutlet("server_address", &this->addressLabel),
    MakeOutlet("server_hint", &this->hintLabel),
    MakeOutlet("server_grid", &this->detailGrid),
    MakeOutlet("server_source", &this->sourceLabel),
    MakeOutlet("server_map", &this->mapLabel),
    MakeOutlet("server_gameplay", &this->gameplayLabel),
    MakeOutlet("server_movement", &this->movementLabel),
    MakeOutlet("server_mapshot", &this->mapshotView),
    MakeOutlet("server_players", &this->playersLabel),
    MakeOutlet("server_ping", &this->pingLabel),
    MakeOutlet("connect", &this->connectButton),
    MakeOutlet("quickJoin", &quickJoin),
    MakeOutlet("refresh", &refresh),
    MakeOutlet("hideEmpty", &hideEmpty),
    MakeOutlet("hideBots", &hideBots),
    MakeOutlet("maxPing", &this->maxPingSlider)
  );

  $(self->view, awakeWithResourceName, "ui/play/JoinServerViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/play/JoinServerViewController.css");
  assert(self->view->stylesheet);

  $(this->serversTableView, addColumnWithIdentifier, _server);
  $(this->serversTableView, addColumnWithIdentifier, _map);
  $(this->serversTableView, addColumnWithIdentifier, _players);
  $(this->serversTableView, addColumnWithIdentifier, _ping);

  this->serversTableView->dataSource.numberOfRows = numberOfRows;
  this->serversTableView->dataSource.self = this;

  this->serversTableView->delegate.cellForColumnAndRow = cellForColumnAndRow;
  this->serversTableView->delegate.didSetSortColumn = didSetSortColumn;
  this->serversTableView->delegate.didSelectRowsAtIndexes = didSelectRowsAtIndexes;
  this->serversTableView->delegate.self = this;

  hideBots->delegate.didToggle = didToggleHideBots;
  hideBots->delegate.self = this;

  hideEmpty->delegate.didToggle = didToggleHideEmpty;
  hideEmpty->delegate.self = this;

  this->maxPingSlider->delegate.didSetValue = didSetMaxPing;
  this->maxPingSlider->delegate.self = this;
  $(this->maxPingSlider, setLabelFormat, "%g ms");

  refresh->delegate.didClick = didClickRefresh;
  refresh->delegate.self = this;

  this->connectButton->delegate.didClick = didClickConnect;
  this->connectButton->delegate.self = this;

  quickJoin->delegate.didClick = didClickQuickJoin;
  quickJoin->delegate.self = this;

  refreshDetails(this);
}

/**
 * @see ViewController::respondToEvent(ViewController *, const SDL_Event *)
 */
static void respondToEvent(ViewController *self, const SDL_Event *event) {

  if (event->type == MVC_NOTIFICATION_EVENT && event->user.code == NOTIFICATION_SERVER_PARSED) {
    $((JoinServerViewController *) self, reloadServers);
  }

  super(ViewController, self, respondToEvent, event);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  super(ViewController, self, viewWillAppear);

  JoinServerViewController *this = (JoinServerViewController *) self;

  $(this, reloadServers); // show what we already know at once, then ask the master again

  cgi.GetServers();
}

#pragma mark - JoinServerViewController

/**
 * @brief Comparator for server sorting.
 */
static Order comparator(const ident a, const ident b) {

  JoinServerViewController *this = sortingJoinServerViewController;
  const TableColumn *sortColumn = this->serversTableView->sortColumn;

  // an unanswered server goes last, whichever way the ping column points
  if (sortColumn && q_strcmp(sortColumn->identifier, _ping) == 0) {
    const bool leftUnanswered = pingUnanswered((const cl_server_info_t *) a);
    const bool rightUnanswered = pingUnanswered((const cl_server_info_t *) b);
    if (leftUnanswered != rightUnanswered) {
      return leftUnanswered ? OrderDescending : OrderAscending;
    }
  }

  if (sortColumn) {
    const cl_server_info_t *s0, *s1;

    switch (sortColumn->order) {
      case OrderAscending:
        s0 = a; s1 = b;
        break;
      case OrderDescending:
        s0 = b; s1 = a;
        break;
      default:
        return OrderSame;
    }

    int32_t cmp = 0;

    if (q_strcmp(sortColumn->identifier, _server) == 0) {
      cmp = q_strcmp(s0->hostname, s1->hostname);
    } else if (q_strcmp(sortColumn->identifier, _map) == 0) {
      cmp = q_strcmp(s0->name, s1->name);
    } else if (q_strcmp(sortColumn->identifier, _players) == 0) {
      cmp = s0->clients - s1->clients;
    } else if (q_strcmp(sortColumn->identifier, _ping) == 0) {
      cmp = s0->ping - s1->ping;
    } else {
      assert(false);
    }

    return cmp < 0 ? OrderAscending : cmp > 0 ? OrderDescending : OrderSame;
  }

  return OrderSame;
}

/**
 * @fn void JoinServerViewController::reloadServers(JoinServerViewController *self)
 * @memberof JoinServerViewController
 */
static void reloadServers(JoinServerViewController *self) {

  release(self->servers);

  self->servers = $(alloc(PointerArray), init);

  const PointerArray *servers = cgi.Servers();
  const size_t count = servers ? servers->count : 0;

  Cg_Debug("%d servers known to the client\n", (int32_t) count);

  uint32_t hidden = 0;

  for (size_t i = 0; i < count; i++) {
    cl_server_info_t *server = $(servers, get, i);

    const int32_t clients = cg_join_server_hide_bots->value ? server->clients - server->bots : server->clients;

    if (clients == 0 && (cg_join_server_hide_empty->value || cg_join_server_hide_bots->value)) {
      Cg_Debug("Hiding %s: %d clients, %d bots, hide_empty %d, hide_bots %d\n",
               server->hostname, server->clients, server->bots,
               cg_join_server_hide_empty->integer, cg_join_server_hide_bots->integer);

      hidden++;
      continue;
    }

    $(self->servers, add, server);
  }

  Cg_Debug("Showing %d servers, %u hidden by filters\n", (int32_t) self->servers->count, hidden);

  sortingJoinServerViewController = self;
  $(self->servers, sort, comparator);
  sortingJoinServerViewController = NULL;

  $((View *) self->emptyLabel, setHidden, self->servers->count > 0);
  $((View *) self->serversTableView, setHidden, self->servers->count == 0);

  $(self->serversTableView, reloadData);

  restoreSelection(self);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->respondToEvent = respondToEvent;
  ((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;

  ((JoinServerViewControllerInterface *) clazz->interface)->reloadServers = reloadServers;

  cg_join_server_hide_empty = cgi.AddCvar("cg_join_server_hide_empty", "0", CVAR_ARCHIVE, NULL);
  cg_join_server_hide_bots = cgi.AddCvar("cg_join_server_hide_bots", "0", CVAR_ARCHIVE, NULL);
}

/**
 * @fn Class *JoinServerViewController::_JoinServerViewController(void)
 * @memberof JoinServerViewController
 */
Class *_JoinServerViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "JoinServerViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(JoinServerViewController),
      .interfaceOffset = offsetof(JoinServerViewController, interface),
      .interfaceSize = sizeof(JoinServerViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
