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

#include "TestScrollViewController.h"

#include <ObjectivelyMVC/Button.h>
#include <ObjectivelyMVC/Checkbox.h>
#include <ObjectivelyMVC/ScrollView.h>
#include <ObjectivelyMVC/Scrollbar.h>
#include <ObjectivelyMVC/Select.h>
#include <ObjectivelyMVC/StackView.h>
#include <ObjectivelyMVC/TableView.h>
#include <ObjectivelyMVC/TextView.h>

#define _Class _TestScrollViewController

#define TEST_SCROLLBAR_WIDTH 14

#pragma mark - Delegates

/**
 * @brief ButtonDelegate; pops this test harness off the view controller stack.
 */
static void didClickClose(Button *button) {
  cgi.PopViewController();
}

/**
 * @brief ButtonDelegate; the plain Button row -- just proves clicks land.
 */
static void didClickTestButton(Button *button) {
  Cg_Debug("Test button clicked\n");
}

/**
 * @brief CheckboxDelegate.
 */
static void didToggleTestCheckbox(Checkbox *checkbox) {
  Cg_Debug("Test checkbox toggled: %d\n", ((Control *) checkbox)->state & ControlStateSelected ? 1 : 0);
}

/**
 * @brief SelectDelegate, shared by both the plain dropdown and the
 * single-selection "radio group" Select below.
 */
static void didSelectTestOption(Select *select, Option *option) {
  Cg_Debug("Test select chose: %s\n", option->title->text ?: "");
}

#pragma mark - TableView demo (JD's KeyValueView-via-TableView idea)

/**
 * @brief Sample key/value pairs for the TableView demo row.
 */
static const char *test_table_data[][2] = {
  { "blend",   "src_alpha one_minus_src_alpha" },
  { "scale",   "1.0 1.0" },
  { "rotate",  "0" },
  { "color",   "1.0 1.0 1.0" },
};

/**
 * @see TableViewDataSource::numberOfRows(const TableView *)
 */
static size_t testTable_numberOfRows(const TableView *tableView) {
  return lengthof(test_table_data);
}

/**
 * @see TableViewDataSource::valueForColumnAndRow(const TableView *, const TableColumn *, size_t)
 */
static ident testTable_valueForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {
  const int col = !strcmp(column->identifier, "value");
  return (ident) test_table_data[row][col];
}

/**
 * @see TableViewDelegate::cellForColumnAndRow(const TableView *, const TableColumn *, size_t)
 */
static TableCellView *testTable_cellForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

  TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);
  assert(cell);

  const char *value = (const char *) testTable_valueForColumnAndRow(tableView, column, row);
  $(cell->text, setText, value);

  return cell;
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  View *box, *viewport;
  StackView *header, *rows;
  Button *close, *button;
  TextView *textView;
  Select *select, *radio;
  Checkbox *checkbox;
  TableView *table;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("box", &box),
    MakeOutlet("header", &header),
    MakeOutlet("close", &close),
    MakeOutlet("viewport", &viewport),
    MakeOutlet("rows", &rows),
    MakeOutlet("textView", &textView),
    MakeOutlet("select", &select),
    MakeOutlet("checkbox", &checkbox),
    MakeOutlet("button", &button),
    MakeOutlet("radio", &radio),
    MakeOutlet("table", &table)
  );

  $(self->view, awakeWithResourceName, "ui/editor/TestScrollViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/editor/TestScrollViewController.css");
  assert(self->view->stylesheet);

  close->delegate.self = self;
  close->delegate.didClick = didClickClose;

  button->delegate.self = self;
  button->delegate.didClick = didClickTestButton;

  checkbox->delegate.self = self;
  checkbox->delegate.didToggle = didToggleTestCheckbox;

  $(select, addOption, "Option A", (ident) "a");
  $(select, addOption, "Option B", (ident) "b");
  $(select, addOption, "Option C", (ident) "c");
  select->delegate.self = self;
  select->delegate.didSelectOption = didSelectTestOption;

  // MVC has no dedicated radio-button class; a single-selection Select is the
  // closest equivalent (mutually exclusive choice from a fixed set).
  ((Control *) radio)->selection = ControlSelectionSingle;
  $(radio, addOption, "Low", (ident) "low");
  $(radio, addOption, "Medium", (ident) "medium");
  $(radio, addOption, "High", (ident) "high");
  radio->delegate.self = self;
  radio->delegate.didSelectOption = didSelectTestOption;

  $(table, addColumnWithIdentifier, "key");
  $(table, addColumnWithIdentifier, "value");
  table->dataSource.self = table;
  table->dataSource.numberOfRows = testTable_numberOfRows;
  table->dataSource.valueForColumnAndRow = testTable_valueForColumnAndRow;
  table->delegate.self = table;
  table->delegate.cellForColumnAndRow = testTable_cellForColumnAndRow;
  $(table, reloadData);

  (void) textView;

  // Wrap `rows` in a ScrollView + pin a Scrollbar to it, mirroring
  // MaterialViewController's stages list -- scrolling lives on ScrollView
  // (generic over any contentView), not on the StackView itself.
  retain((View *) rows);
  $((View *) rows, removeFromSuperview);
  ((View *) rows)->autoresizingMask = ViewAutoresizingContain | ViewAutoresizingWidth;

  ScrollView *scrollView = $(alloc(ScrollView), initWithFrame, NULL);
  assert(scrollView);
  ((View *) scrollView)->autoresizingMask = ViewAutoresizingFill;
  ((View *) scrollView)->clipsSubviews = true;
  ((View *) scrollView)->padding.right = TEST_SCROLLBAR_WIDTH + 4;
  $(scrollView, setContentView, (View *) rows);
  release((View *) rows);
  $(viewport, addSubview, (View *) scrollView);

  Scrollbar *scrollbar = $(alloc(Scrollbar), initWithScrollView, scrollView);
  assert(scrollbar);
  ((View *) scrollbar)->frame.w = TEST_SCROLLBAR_WIDTH;
  ((View *) scrollbar)->autoresizingMask = ViewAutoresizingHeight;
  ((View *) scrollbar)->alignment = ViewAlignmentRight;
  $(viewport, addSubview, (View *) scrollbar);
  release(scrollbar);
  release(scrollView);

  // Size the viewport to fill the (fixed-size) box below the header. A
  // "fixed size parent, one child eats the leftover height" layout isn't
  // expressible in pure CSS here -- the same accepted exception the real
  // editor's stagesViewport already relies on.
  $((View *) header, layoutIfNeeded);
  const SDL_Size headerSize = $((View *) header, sizeThatContains);
  const SDL_Rect boxBounds = $(box, bounds);
  viewport->frame = MakeRect(0, headerSize.h + 10, boxBounds.w, boxBounds.h - headerSize.h - 10);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

/**
 * @fn Class *TestScrollViewController::_TestScrollViewController(void)
 * @memberof TestScrollViewController
 */
Class *_TestScrollViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "TestScrollViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(TestScrollViewController),
      .interfaceOffset = offsetof(TestScrollViewController, interface),
      .interfaceSize = sizeof(TestScrollViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
