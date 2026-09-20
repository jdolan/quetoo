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

#pragma once

#include "g_local.h"

typedef bool (*GridKdTreeFilter)(size_t nodenum, void *data, float *distance);

struct GHeapEntry {
  float cost;
  void *data;
};

struct GHeap {
  size_t count;
  size_t capacity;
  struct GHeapEntry entries[];
};

struct KdTreeNode {
  size_t nodenum;
  struct KdTreeNode *left;
  struct KdTreeNode *right;
};

struct GridKdTree {
  size_t nodecount;
  size_t capacity;
  Vec3 *srcdata;
  struct KdTreeNode *root;
  struct KdTreeNode nodes[];
};

void gridkdtree_free(struct GridKdTree **tree);
struct GridKdTree *gridkdtree_create(Vec3 *srcdata, size_t count);
size_t gridkdtree_query_filter(struct GridKdTree *tree, const Vec3 querypos, float maxDistance,
                               GridKdTreeFilter filter, void *data);

struct GHeap *gheap_create(size_t capacity);
void gheap_free(struct GHeap **heap);
bool gheap_push(struct GHeap *heap, float cost, void *data);
void *gheap_pop(struct GHeap *heap);
void gheap_reset(struct GHeap *heap);
