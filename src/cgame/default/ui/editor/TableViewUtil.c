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

#include <assert.h>

#include "TableViewUtil.h"

/**
 * @brief Wraps `widget` in a TableCellView (hiding the cell's default, empty Text).
 */
static TableCellView *makeCell(View *widget) {

  TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);
  assert(cell);

  ((View *) cell->text)->hidden = true;
  $((View *) cell, addSubview, widget);

  return cell;
}

/**
 * @see TableViewUtil.h
 */
void TableView_AddRow(TableView *table, View *key, View *value, int keyWidth, int valueWidth) {

  assert(table);
  assert(key);
  assert(value);

  // None of these tables are sortable spreadsheets; keep the header collapsed.
  ((View *) table->headerView)->hidden = true;

  TableRowView *row = $(alloc(TableRowView), initWithTableView, table);
  assert(row);

  // The key column is fixed width; the value column is the flexible one --
  // TableRowView's own layoutSubviews stretches the row's last cell to fill
  // whatever width remains after the key column, every layout pass. Only a
  // floor is set here (no max), so that stretch is never clamped back down.
  key->minSize.w = key->maxSize.w = keyWidth;
  value->minSize.w = valueWidth;

  TableCellView *keyCell = makeCell(key);
  $(row, addCell, keyCell);
  release(keyCell);

  TableCellView *valueCell = makeCell(value);
  $(row, addCell, valueCell);
  release(valueCell);

  $(table->rows, addObject, row);
  release(row);

  $((View *) table->contentView, addSubview, (View *) row);

  ((View *) table)->needsLayout = true;
}

/**
 * @brief ArrayEnumerator to remove TableRowViews from the table's contentView.
 */
static void removeAllRows_enumerate(const Array *array, ident obj, ident data) {
  $((View *) data, removeSubview, (View *) obj);
}

/**
 * @see TableViewUtil.h
 */
void TableView_RemoveAllRows(TableView *table) {

  assert(table);

  $((Array *) table->rows, removeAllObjectsWithEnumerator, removeAllRows_enumerate, table->contentView);

  ((View *) table)->needsLayout = true;
}
