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

static const char *_hostname = "Hostname";
static const char *_source = "Source";
static const char *_name = "Map";
static const char *_gameplay = "Gameplay";
static const char *_players = "Players";
static const char *_ping = "Ping";

static cvar_t *cg_join_server_hide_empty;
static cvar_t *cg_join_server_hide_bots;
static JoinServerViewController *sortingJoinServerViewController;

#define _Class _JoinServerViewController

static const cl_server_info_t *serverAtIndex(const List *servers, size_t index) {

  if (servers == NULL) {
    return NULL;
  }

  const ListNode *node = servers->head;
  while (node && index--) {
    node = node->next;
  }

  return node ? node->data : NULL;
}

#pragma mark - Delegates

/**
 * @brief CheckboxDelegate for Hide Bots.
 */
static void didToggleHideBots(Checkbox *checkbox) {

  cvarCheckboxDidToggle(checkbox);

  JoinServerViewController *this = checkbox->delegate.self;

  $(this, reloadServers);
}

/**
 * @brief CheckboxDelegate for Hide Empty.
 */
static void didToggleHideEmpty(Checkbox *checkbox) {

  cvarCheckboxDidToggle(checkbox);

  JoinServerViewController *this = checkbox->delegate.self;

  $(this, reloadServers);
}

/**
 * @brief ButtonDelegate for Quick Join.
 * @description Selects a server based on minumum ping and maximum players with
 * a bit of lovely random thrown in. Any server that matches the criteria will
 * be weighted by how much "better" they are by how much lower their ping is and
 * how many more players there are.
 */
static void didClickQuickJoin(Button *button) {

  JoinServerViewController *this = button->delegate.self;

  const int32_t max_ping = Clampf(cg_quick_join_max_ping->integer, 0, 999);
  const int32_t min_clients = Clampf(cg_quick_join_min_clients->integer, 0, MAX_CLIENTS);

  uint32_t total_weight = 0;

  const ListNode *node = this->servers ? this->servers->head : NULL;

  while (node != NULL) {
    const cl_server_info_t *server = node->data;

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

    node = node->next;
  }

  if (total_weight == 0) {
    return;
  }

  node = this->servers ? this->servers->head : NULL;

  const uint32_t random_weight = RandomRangeu(0, total_weight);
  uint32_t current_weight = 0;

  while (node != NULL) {
    const cl_server_info_t *server = node->data;

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

    node = node->next;
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
 */
static void didClickConnect(Button *button) {

  JoinServerViewController *this = button->delegate.self;

  IndexSet *selectedRowIndexes = $((TableView *) this->serversTableView, selectedRowIndexes);
  if (selectedRowIndexes->count) {

    const uint32_t index = (uint32_t) selectedRowIndexes->indexes[0];
    const cl_server_info_t *server = serverAtIndex(this->servers, index);

    if (server) {
      cgi.Connect(&server->addr);
    }
  }

  release(selectedRowIndexes);
}

#pragma mark - TableViewDataSource

/**
 * @see TableViewDataSource::numberOfRows
 */
static size_t numberOfRows(const TableView *tableView) {

  JoinServerViewController *this = tableView->dataSource.self;

  return this->servers ? this->servers->count : 0;
}

/**
 * @see TableViewDataSource::valueForColumnAndRow
 */
static ident valueForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

  JoinServerViewController *this = tableView->dataSource.self;

  cl_server_info_t *server = (cl_server_info_t *) serverAtIndex(this->servers, row);
  assert(server);

  if (strcmp(column->identifier, _hostname) == 0) {
    return server->hostname;
  } else if (strcmp(column->identifier, _source) == 0) {
    return &server->source;
  } else if (strcmp(column->identifier, _name) == 0) {
    return server->name;
  } else if (strcmp(column->identifier, _gameplay) == 0) {
    return server->gameplay;
  } else if (strcmp(column->identifier, _players) == 0) {
    return &server->clients;
  } else if (strcmp(column->identifier, _ping) == 0) {
    return &server->ping;
  }

  return NULL;
}

#pragma mark - TableViewDelegate

/**
 * @see TableViewDelegate::cellForColumnAndRow
 */
static TableCellView *cellForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

  JoinServerViewController *this = tableView->dataSource.self;

  cl_server_info_t *server = (cl_server_info_t *) serverAtIndex(this->servers, row);
  assert(server);

  TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);

  if (strlen(server->error)) {
    if (strcmp(column->identifier, _hostname) == 0) {
      $(cell->text, setText, server->error);
      $((View *) cell, addClassName, "error");
    } else {
      $(cell->text, setText, NULL);
    }
  } else {
    if (strcmp(column->identifier, _hostname) == 0) {
      $(cell->text, setText, server->hostname);
    } else if (strcmp(column->identifier, _source) == 0) {
      switch (server->source) {
        case SERVER_SOURCE_INTERNET:
          $(cell->text, setText, "Internet");
          break;
        case SERVER_SOURCE_USER:
          $(cell->text, setText, "User");
          break;
        case SERVER_SOURCE_BCAST:
          $(cell->text, setText, "LAN");
          break;
      }
    } else if (strcmp(column->identifier, _name) == 0) {
      $(cell->text, setText, server->name);
    } else if (strcmp(column->identifier, _gameplay) == 0) {
      $(cell->text, setText, server->gameplay);
    } else if (strcmp(column->identifier, _players) == 0) {
      if (cg_join_server_hide_bots->value) {
        $(cell->text, setText, va("%d/%d", server->clients - server->bots, server->max_clients));
      } else {
        $(cell->text, setText, va("%d/%d", server->clients, server->max_clients));
      }
    } else if (strcmp(column->identifier, _ping) == 0) {
      $(cell->text, setText, va("%3d", server->ping));
    }
  }

  return cell;
}

/**
 * @see TableViewDelegate::didSetSortColumn(TableView *)
 */
static void didSetSortColumn(TableView *tableView) {
  $((JoinServerViewController *) tableView->delegate.self, reloadServers);
}

/**
 * @see TableViewDelegate::didSelectRowsAtIndexes(TableView *, const IndexSet *)
 */
static void didSelectRowsAtIndexes(TableView *tableView, const IndexSet *indexes) {

  JoinServerViewController *this = tableView->delegate.self;

  View *view = (View *) tableView;

  const SDL_PropertiesID props = SDL_GetWindowProperties(view->window);
  const SDL_Event *event = SDL_GetPointerProperty(props, "event", NULL);
  if (event && event->button.clicks == 2) {

    const uint32_t index = (uint32_t) indexes->indexes[0];
    const cl_server_info_t *server = serverAtIndex(this->servers, index);

    if (server) {
      cgi.Connect(&server->addr);
    }
  }
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
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
  Button *quickJoin, *refresh, *connect;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("servers", &this->serversTableView),
    MakeOutlet("hideEmpty", &hideEmpty),
    MakeOutlet("hideBots", &hideBots),
    MakeOutlet("quickJoin", &quickJoin),
    MakeOutlet("refresh", &refresh),
    MakeOutlet("connect", &connect)
  );

  $(self->view, awakeWithResourceName, "ui/play/JoinServerViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/play/JoinServerViewController.css");
  assert(self->view->stylesheet);
  
  $(this->serversTableView, addColumnWithIdentifier, _hostname);
  $(this->serversTableView, addColumnWithIdentifier, _source);
  $(this->serversTableView, addColumnWithIdentifier, _name);
  $(this->serversTableView, addColumnWithIdentifier, _gameplay);
  $(this->serversTableView, addColumnWithIdentifier, _players);
  $(this->serversTableView, addColumnWithIdentifier, _ping);

  this->serversTableView->dataSource.numberOfRows = numberOfRows;
  this->serversTableView->dataSource.valueForColumnAndRow = valueForColumnAndRow;
  this->serversTableView->dataSource.self = this;

  this->serversTableView->delegate.cellForColumnAndRow = cellForColumnAndRow;
  this->serversTableView->delegate.didSetSortColumn = didSetSortColumn;
  this->serversTableView->delegate.didSelectRowsAtIndexes = didSelectRowsAtIndexes;
  this->serversTableView->delegate.self = this;

  hideBots->delegate.didToggle = didToggleHideBots;
  hideBots->delegate.self = this;

  hideEmpty->delegate.didToggle = didToggleHideEmpty;
  hideEmpty->delegate.self = this;

  quickJoin->delegate.didClick = didClickQuickJoin;
  quickJoin->delegate.self = this;

  refresh->delegate.didClick = didClickRefresh;
  refresh->delegate.self = this;

  connect->delegate.didClick = didClickConnect;
  connect->delegate.self = this;
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

  if (this->servers == NULL) {
    cgi.GetServers();
  } else {
    $(this, reloadServers);
  }
}

#pragma mark - JoinServerViewController

/**
 * @brief GCompareDataFunc for server sorting.
 */
static Order comparator(const ident a, const ident b) {

  JoinServerViewController *this = sortingJoinServerViewController;

  if (this->serversTableView->sortColumn) {
    const cl_server_info_t *s0, *s1;

    switch (this->serversTableView->sortColumn->order) {
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

    if (strcmp(this->serversTableView->sortColumn->identifier, _hostname) == 0) {
      cmp = strcmp(s0->hostname, s1->hostname);
    } else if (strcmp(this->serversTableView->sortColumn->identifier, _source) == 0) {
      cmp = s0->source - s1->source;
    } else if (strcmp(this->serversTableView->sortColumn->identifier, _name) == 0) {
      cmp = strcmp(s0->name, s1->name);
    } else if (strcmp(this->serversTableView->sortColumn->identifier, _gameplay) == 0) {
      cmp = strcmp(s0->gameplay, s1->gameplay);
    } else if (strcmp(this->serversTableView->sortColumn->identifier, _players) == 0) {
      cmp = s0->clients - s1->clients;
    } else if (strcmp(this->serversTableView->sortColumn->identifier, _ping) == 0) {
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

  self->servers = $(alloc(List), init);

  const List *servers = cgi.Servers();
  for (const ListNode *node = servers ? servers->head : NULL; node; node = node->next) {
    cl_server_info_t *server = node->data;
    $(self->servers, append, server);
  }

  for (ListNode *node = self->servers->head; node; ) {
    ListNode *next = node->next;

    cl_server_info_t *server = node->data;

    const int32_t clients = cg_join_server_hide_bots->value ? server->clients - server->bots : server->clients;

    if (clients == 0 && (cg_join_server_hide_empty->value || cg_join_server_hide_bots->value)) {
      $(self->servers, removeNode, node);
    }

    node = next;
  }

  sortingJoinServerViewController = self;
  $(self->servers, sort, comparator);
  sortingJoinServerViewController = NULL;

  $(self->serversTableView, reloadData);
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
