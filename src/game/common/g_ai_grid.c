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

#include "g_local.h"

void G_Ai_KdTreeFree(struct GameAiKdTree **tree) {

  if (!*tree) {
    return;
  }

  free(*tree);
  *tree = NULL;
}

static _Thread_local struct GameAiKdTreeSortContext {
  uint8_t dim;
  Vec3 *srcdata;
} sortContext;

struct GameAiKdTreeBuildContext {
  struct GameAiKdTree *tree;
  int32_t *sortedx;
  size_t slicesize;
};

static int32_t G_Ai_KdTreeCompare(const void *_l, const void *_r) {
  const int32_t l = *(const int32_t *) _l;
  const int32_t r = *(const int32_t *) _r;
  const Vec3 *srcdata = sortContext.srcdata;
  const uint8_t dim = sortContext.dim;

  if (srcdata[l].xyz[dim] < srcdata[r].xyz[dim]) {
    return -1;
  }
  if (srcdata[l].xyz[dim] > srcdata[r].xyz[dim]) {
    return 1;
  }

  return 0;
}

static struct GameAiKdTreeNode *G_Ai_KdTreeAllocNode(struct GameAiKdTree *tree) {

  if (tree->nodecount < tree->capacity) {
    return &tree->nodes[tree->nodecount++];
  }

  return NULL;
}

static struct GameAiKdTreeNode *G_Ai_KdTreeBuild(struct GameAiKdTreeBuildContext *ctx, const size_t dim) {

  if (ctx->slicesize == 0) {
    return NULL;
  }

  sortContext.srcdata = ctx->tree->srcdata;
  sortContext.dim = dim;
  qsort(ctx->sortedx, ctx->slicesize, sizeof(ctx->sortedx[0]), G_Ai_KdTreeCompare);

  struct GameAiKdTreeNode *node = G_Ai_KdTreeAllocNode(ctx->tree);
  const size_t mid = (ctx->slicesize - 1) / 2;
  const size_t nextDim = (dim + 1) % 3;

  node->nodenum = ctx->sortedx[mid];
  node->left = G_Ai_KdTreeBuild(&(struct GameAiKdTreeBuildContext) {
    .tree = ctx->tree,
    .sortedx = ctx->sortedx,
    .slicesize = mid
  }, nextDim);
  node->right = G_Ai_KdTreeBuild(&(struct GameAiKdTreeBuildContext) {
    .tree = ctx->tree,
    .sortedx = ctx->sortedx + mid + 1,
    .slicesize = ctx->slicesize - mid - 1
  }, nextDim);

  return node;
}

struct GameAiKdTree *G_Ai_KdTreeCreate(Vec3 *srcdata, const size_t count) {
  int32_t *sortedx = malloc(count * sizeof(int32_t));
  const size_t capacity = count;
  const size_t size = sizeof(struct GameAiKdTree) + capacity * sizeof(struct GameAiKdTreeNode);
  struct GameAiKdTree *tree = malloc(size);

  if (!tree || !sortedx) {
    free(tree);
    free(sortedx);
    return NULL;
  }

  for (size_t i = 0; i < count; i++) {
    sortedx[i] = (int32_t) i;
  }

  for (size_t i = 0; i < capacity; i++) {
    tree->nodes[i].nodenum = SIZE_MAX;
    tree->nodes[i].left = NULL;
    tree->nodes[i].right = NULL;
  }

  tree->srcdata = srcdata;
  tree->nodecount = 0;
  tree->capacity = capacity;

  tree->root = G_Ai_KdTreeBuild(&(struct GameAiKdTreeBuildContext) {
    .tree = tree,
    .sortedx = sortedx,
    .slicesize = count
  }, 0);

  free(sortedx);
  return tree;
}
struct GameAiKdTreeQueryContext {
  struct GameAiKdTree *tree;
  Vec3 querypos;
  double bestdist;
  size_t best;
  GameAiKdTreeFilter filter;
  void *data;
};

static void G_Ai_KdTreeQueryNode(struct GameAiKdTreeQueryContext *ctx, const struct GameAiKdTreeNode *node, const size_t dim) {

  if (node == NULL || node->nodenum == SIZE_MAX) {
    return;
  }

  const Vec3 pos = ctx->tree->srcdata[node->nodenum];
  float filterDist = INFINITY;

  if (ctx->filter(node->nodenum, ctx->data, &filterDist) && filterDist < ctx->bestdist) {
    ctx->best = node->nodenum;
    ctx->bestdist = filterDist;
  }

  const double sdist = ctx->querypos.xyz[dim] - pos.xyz[dim];
  const struct GameAiKdTreeNode *nearNode = sdist < 0 ? node->left : node->right;
  const struct GameAiKdTreeNode *farNode = sdist < 0 ? node->right : node->left;
  const size_t nextDim = (dim + 1) % 3;

  G_Ai_KdTreeQueryNode(ctx, nearNode, nextDim);

  if (sdist * sdist <= ctx->bestdist) {
    G_Ai_KdTreeQueryNode(ctx, farNode, nextDim);
  }
}

size_t G_Ai_KdTreeQuery(struct GameAiKdTree *tree, const Vec3 querypos, const float maxDistance,
                        GameAiKdTreeFilter filter, void *data) {

  if (!tree || !tree->root || !filter || maxDistance <= 0.f) {
    return SIZE_MAX;
  }

  struct GameAiKdTreeQueryContext ctx = {
    .tree = tree,
    .querypos = querypos,
    .bestdist = (double) maxDistance * maxDistance,
    .best = SIZE_MAX,
    .filter = filter,
    .data = data
  };

  G_Ai_KdTreeQueryNode(&ctx, tree->root, 0);
  return ctx.best;
}

struct GameAiHeap *G_Ai_HeapCreate(const size_t capacity) {
  struct GameAiHeap *ret = malloc(sizeof(struct GameAiHeap) + sizeof(struct GameAiHeapEntry) * capacity);

  if (ret == NULL) {
    return NULL;
  }

  for (size_t i = 0; i < capacity; i++) {
    ret->entries[i].cost = INFINITY;
    ret->entries[i].data = NULL;
  }

  ret->capacity = capacity;
  ret->count = 0;
  return ret;
}

void G_Ai_HeapFree(struct GameAiHeap **heap) {

  if (!*heap) {
    return;
  }

  free(*heap);
  *heap = NULL;
}

static void G_Ai_HeapSwap(struct GameAiHeapEntry *a, struct GameAiHeapEntry *b) {
  struct GameAiHeapEntry tmp = *a;
  *a = *b;
  *b = tmp;
}

bool G_Ai_HeapPush(struct GameAiHeap *heap, const float cost, void *data) {
  assert(data != NULL);

  if (heap->count >= heap->capacity) {
    return false;
  }

  heap->count++;
  size_t i = heap->count - 1;
  heap->entries[i].cost = cost;
  heap->entries[i].data = data;

  if (i == 0) {
    return true;
  }

  while (i != 0) {
    const size_t parentIndex = (i - 1) / 2;
    struct GameAiHeapEntry *parent = &heap->entries[parentIndex];

    if (heap->entries[i].cost < parent->cost) {
      G_Ai_HeapSwap(&heap->entries[i], parent);
      i = parentIndex;
    } else {
      break;
    }
  }

  return true;
}

static float G_Ai_HeapPeekCost(const struct GameAiHeap *heap, const size_t node) {

  if (node >= heap->count) {
    return INFINITY;
  }

  const struct GameAiHeapEntry *entry = &heap->entries[node];
  return entry->cost;
}

static struct GameAiHeapEntry G_Ai_HeapPopEntry(struct GameAiHeap *heap) {

  if (heap->count == 0) {
    return (struct GameAiHeapEntry) { INFINITY, NULL };
  }

  const struct GameAiHeapEntry ret = heap->entries[0];
  heap->count--;
  heap->entries[0] = heap->entries[heap->count];

  size_t node = 0;
  for (;;) {
    const size_t left = node * 2 + 1;
    const size_t right = node * 2 + 2;

    if (node >= heap->count) {
      break;
    }

    struct GameAiHeapEntry *cur = &heap->entries[node];
    size_t smallest = node;

    if (left < heap->count && G_Ai_HeapPeekCost(heap, left) < cur->cost) {
      smallest = left;
    }

    if (right < heap->count && G_Ai_HeapPeekCost(heap, right) < heap->entries[smallest].cost) {
      smallest = right;
    }

    if (smallest == node) {
      break;
    }

    G_Ai_HeapSwap(cur, &heap->entries[smallest]);
    node = smallest;
  }

  return ret;
}

void G_Ai_HeapReset(struct GameAiHeap *heap) {
  heap->count = 0;
}

void *G_Ai_HeapPop(struct GameAiHeap *heap) {
  return G_Ai_HeapPopEntry(heap).data;
}
