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

#pragma once

#include <ObjectivelyMVC/TableView.h>

/**
 * @file
 * @brief Reproduces the old KeyValueTableView::addRow/removeAllRows ergonomics on top of a
 * plain TableView, for editor tables whose rows are built incrementally in C rather than
 * through a `dataSource`/`delegate` (genuinely dynamic content: an unknown-in-advance row
 * count, or heterogeneous per-row widgets chosen at runtime). Tables with a fixed,
 * known-in-advance shape should be declared in JSON via TableView's `"cells"` array instead.
 */

/**
 * @brief Appends a row to `table` pairing `key` and `value`, bypassing
 * `dataSource`/`delegate`/`reloadData` entirely (direct TableRowView/TableCellView
 * construction). `key` is clamped to a fixed width (`minSize.w = maxSize.w`); `value`
 * only gets a floor (`minSize.w`) and is stretched to fill the table's remaining
 * width every layout pass (see TableRowView::layoutSubviews).
 * @param table The TableView (2 columns; column identifiers are not used for row placement).
 * @param key The View for the row's first (key) cell.
 * @param value The View for the row's second (value) cell.
 * @param keyWidth The fixed width to clamp `key` to.
 * @param valueWidth The minimum width for `value` (it stretches wider when there's room).
 */
void TableView_AddRow(TableView *table, View *key, View *value, int keyWidth, int valueWidth);

/**
 * @brief Removes all rows previously added via `TableView_AddRow`.
 * @param table The TableView.
 */
void TableView_RemoveAllRows(TableView *table);
