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

typedef bool (*GameAiKdTreeFilter)(size_t nodenum, void *data, float *distance);

struct GameAiHeapEntry {
  float cost;
  void *data;
};

struct GameAiHeap {
  size_t count;
  size_t capacity;
  struct GameAiHeapEntry entries[];
};

struct GameAiKdTreeNode {
  size_t nodenum;
  struct GameAiKdTreeNode *left;
  struct GameAiKdTreeNode *right;
};

struct GameAiKdTree {
  size_t nodecount;
  size_t capacity;
  Vec3 *srcdata;
  struct GameAiKdTreeNode *root;
  struct GameAiKdTreeNode nodes[];
};

void G_Ai_KdTreeFree(struct GameAiKdTree **tree);
struct GameAiKdTree *G_Ai_KdTreeCreate(Vec3 *srcdata, size_t count);
size_t G_Ai_KdTreeQuery(struct GameAiKdTree *tree, const Vec3 querypos, float maxDistance,
                        GameAiKdTreeFilter filter, void *data);

struct GameAiHeap *G_Ai_HeapCreate(size_t capacity);
void G_Ai_HeapFree(struct GameAiHeap **heap);
bool G_Ai_HeapPush(struct GameAiHeap *heap, float cost, void *data);
void *G_Ai_HeapPop(struct GameAiHeap *heap);
void G_Ai_HeapReset(struct GameAiHeap *heap);
