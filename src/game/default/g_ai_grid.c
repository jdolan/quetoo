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

#include "g_local.h"

void gridkdtree_free(struct gridkdtree_s **tree) {

  if (!*tree) {
    return;
  }

  free(*tree);
  *tree = NULL;
}

static _Thread_local struct cmpstr_s {
  uint8_t dim;
  vec3_t *srcdata;
} cmpstr;

struct kdtbuildctx_s {
  struct gridkdtree_s *tree;
  int32_t *sortedx;
  size_t slicesize;
};

static int32_t compare_v(const void *_l, const void *_r) {
  const int32_t l = *(const int32_t *) _l;
  const int32_t r = *(const int32_t *) _r;
  const vec3_t *srcdata = cmpstr.srcdata;
  const uint8_t dim = cmpstr.dim;

  if (srcdata[l].xyz[dim] < srcdata[r].xyz[dim]) {
    return -1;
  }
  if (srcdata[l].xyz[dim] > srcdata[r].xyz[dim]) {
    return 1;
  }

  return 0;
}

static struct kdtree_node_s *gkdt_alloc_node(struct gridkdtree_s *tree) {

  if (tree->nodecount < tree->capacity) {
    return &tree->nodes[tree->nodecount++];
  }

  return NULL;
}

static struct kdtree_node_s *kdtree_build(struct kdtbuildctx_s *ctx, const size_t dim) {

  if (ctx->slicesize == 0) {
    return NULL;
  }

  cmpstr.srcdata = ctx->tree->srcdata;
  cmpstr.dim = dim;
  qsort(ctx->sortedx, ctx->slicesize, sizeof(ctx->sortedx[0]), compare_v);

  struct kdtree_node_s *node = gkdt_alloc_node(ctx->tree);
  const size_t mid = (ctx->slicesize - 1) / 2;
  const size_t next_dim = (dim + 1) % 3;

  node->nodenum = ctx->sortedx[mid];
  node->left = kdtree_build(&(struct kdtbuildctx_s) {
    .tree = ctx->tree,
    .sortedx = ctx->sortedx,
    .slicesize = mid
  }, next_dim);
  node->right = kdtree_build(&(struct kdtbuildctx_s) {
    .tree = ctx->tree,
    .sortedx = ctx->sortedx + mid + 1,
    .slicesize = ctx->slicesize - mid - 1
  }, next_dim);

  return node;
}

struct gridkdtree_s *gridkdtree_create(vec3_t *srcdata, const size_t count) {
  int32_t *sortedx = malloc(count * sizeof(int32_t));
  const size_t capacity = count;
  const size_t size = sizeof(struct gridkdtree_s) + capacity * sizeof(struct kdtree_node_s);
  struct gridkdtree_s *tree = malloc(size);

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

  tree->root = kdtree_build(&(struct kdtbuildctx_s) {
    .tree = tree,
    .sortedx = sortedx,
    .slicesize = count
  }, 0);

  free(sortedx);
  return tree;
}
struct kdtree_filter_ctx_s {
  struct gridkdtree_s *tree;
  vec3_t querypos;
  double bestdist;
  size_t best;
  gridkdtree_filter_t filter;
  void *data;
};

static void kdtree_query_filter_rec(struct kdtree_filter_ctx_s *ctx, const struct kdtree_node_s *node, const size_t dim) {

  if (node == NULL || node->nodenum == SIZE_MAX) {
    return;
  }

  const vec3_t pos = ctx->tree->srcdata[node->nodenum];
  float filter_dist = INFINITY;

  if (ctx->filter(node->nodenum, ctx->data, &filter_dist) && filter_dist < ctx->bestdist) {
    ctx->best = node->nodenum;
    ctx->bestdist = filter_dist;
  }

  const double sdist = ctx->querypos.xyz[dim] - pos.xyz[dim];
  const struct kdtree_node_s *near_node = sdist < 0 ? node->left : node->right;
  const struct kdtree_node_s *far_node = sdist < 0 ? node->right : node->left;
  const size_t next_dim = (dim + 1) % 3;

  kdtree_query_filter_rec(ctx, near_node, next_dim);

  if (sdist * sdist <= ctx->bestdist) {
    kdtree_query_filter_rec(ctx, far_node, next_dim);
  }
}

size_t gridkdtree_query_filter(struct gridkdtree_s *tree, const vec3_t querypos, const float max_distance,
                               gridkdtree_filter_t filter, void *data) {

  if (!tree || !tree->root || !filter || max_distance <= 0.f) {
    return SIZE_MAX;
  }

  struct kdtree_filter_ctx_s ctx = {
    .tree = tree,
    .querypos = querypos,
    .bestdist = (double) max_distance * max_distance,
    .best = SIZE_MAX,
    .filter = filter,
    .data = data
  };

  kdtree_query_filter_rec(&ctx, tree->root, 0);
  return ctx.best;
}

struct gheap_s *gheap_create(const size_t capacity) {
  struct gheap_s *ret = malloc(sizeof(struct gheap_s) + sizeof(struct gheap_entry_s) * capacity);

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

void gheap_free(struct gheap_s **heap) {

  if (!*heap) {
    return;
  }

  free(*heap);
  *heap = NULL;
}

static void hswap(struct gheap_entry_s *a, struct gheap_entry_s *b) {
  struct gheap_entry_s tmp = *a;
  *a = *b;
  *b = tmp;
}

bool gheap_push(struct gheap_s *heap, const float cost, void *data) {
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
    const size_t parent_index = (i - 1) / 2;
    struct gheap_entry_s *parent = &heap->entries[parent_index];

    if (heap->entries[i].cost < parent->cost) {
      hswap(&heap->entries[i], parent);
      i = parent_index;
    } else {
      break;
    }
  }

  return true;
}

static float gheap_peek_cost(const struct gheap_s *heap, const size_t node) {

  if (node >= heap->count) {
    return INFINITY;
  }

  const struct gheap_entry_s *entry = &heap->entries[node];
  return entry->cost;
}

static struct gheap_entry_s gheap_pop_inner(struct gheap_s *heap) {

  if (heap->count == 0) {
    return (struct gheap_entry_s) { INFINITY, NULL };
  }

  const struct gheap_entry_s ret = heap->entries[0];
  heap->count--;
  heap->entries[0] = heap->entries[heap->count];

  size_t node = 0;
  for (;;) {
    const size_t left = node * 2 + 1;
    const size_t right = node * 2 + 2;

    if (node >= heap->count) {
      break;
    }

    struct gheap_entry_s *cur = &heap->entries[node];
    size_t smallest = node;

    if (left < heap->count && gheap_peek_cost(heap, left) < cur->cost) {
      smallest = left;
    }

    if (right < heap->count && gheap_peek_cost(heap, right) < heap->entries[smallest].cost) {
      smallest = right;
    }

    if (smallest == node) {
      break;
    }

    hswap(cur, &heap->entries[smallest]);
    node = smallest;
  }

  return ret;
}

void gheap_reset(struct gheap_s *heap) {
  heap->count = 0;
}

void *gheap_pop(struct gheap_s *heap) {
  return gheap_pop_inner(heap).data;
}
