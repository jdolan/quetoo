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

#include "g_local.h"

typedef bool (*gridkdtree_filter_t)(size_t nodenum, void *data, float *distance);

struct gheap_entry_s {
  float cost;
  void *data;
};

struct gheap_s {
  size_t count;
  size_t capacity;
  struct gheap_entry_s entries[];
};

struct kdtree_node_s {
  size_t nodenum;
  struct kdtree_node_s *left;
  struct kdtree_node_s *right;
};

struct gridkdtree_s {
  size_t nodecount;
  size_t capacity;
  vec3_t *srcdata;
  struct kdtree_node_s *root;
  struct kdtree_node_s nodes[];
};

void gridkdtree_free(struct gridkdtree_s **tree);
struct gridkdtree_s *gridkdtree_create(vec3_t *srcdata, size_t count);
size_t gridkdtree_query_filter(struct gridkdtree_s *tree, const vec3_t querypos, float max_distance,
                               gridkdtree_filter_t filter, void *data);

struct gheap_s *gheap_create(size_t capacity);
void gheap_free(struct gheap_s **heap);
bool gheap_push(struct gheap_s *heap, float cost, void *data);
void *gheap_pop(struct gheap_s *heap);
void gheap_reset(struct gheap_s *heap);
