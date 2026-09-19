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

#include "shared/shared.h"

/**
 * @file
 * @brief On-disk formats: demo recordings and MD3 models.
 */

/**
 * @brief Indexes the byte offset of one recorded frame within a demo file.
 * @details Every recorded frame is fully self-contained (delta-encoded against the demo's
 * baselines and a null player state, never against another recorded frame), so this index has
 * one entry per frame and any entry is always a safe, independent seek target.
 */
typedef struct {

  /**
   * @brief The frame number this entry was recorded at.
   */
  int32_t frame_num;

  /**
   * @brief The byte offset of this frame's message within the demo file.
   */
  int32_t offset;
} DemoKeyframe;

/**
 * @brief Format identifier for demo files; rejects files that are not Quetoo demos.
 */
#define DEMO_MAGIC "QDEM"

/**
 * @brief Format version for demo files; rejects demos recorded by an incompatible version.
 */
#define DEMO_VERSION 2

/**
 * @brief The fixed-size header written at offset 0 of every recorded demo file.
 */
typedef struct {

  /**
   * @brief Format identifier; see `DEMO_MAGIC`.
   */
  char magic[4];

  /**
   * @brief Format version; see `DEMO_VERSION`.
   */
  int32_t version;

  /**
   * @brief The path of the map the demo was recorded on, e.g. `maps/edge.bsp`. Kept distinct from
   * `message` because this, not the human-readable name, is the key `Cl_Mapshots` looks up.
   */
  char map[MAX_QPATH];

  /**
   * @brief The human-readable name of the map, from `CS_MESSAGE` (the worldspawn `message`, or
   * the map name where the map defines none). What the demo browser lists demos by.
   */
  char message[MAX_QPATH];

  /**
   * @brief A user-assigned name for this demo, or empty if never renamed. Written in place after
   * the fact, from the demo browser, exactly as `favorite` is.
   */
  char title[MAX_QPATH];

  /**
   * @brief True if the user has starred this demo as a favorite.
   */
  int32_t favorite;

  /**
   * @brief The duration of the demo in milliseconds. Written when recording stops.
   */
  int32_t duration;

  /**
   * @brief The number of entries in the keyframe table. Written when recording stops.
   */
  int32_t num_keyframes;

  /**
   * @brief The byte offset of the keyframe table. Written when recording stops.
   */
  int32_t ofs_keyframes;
} DemoHeader;

/**
 * @brief MD3 file identification.
 */
#define MD3_IDENT          (('3' << 24) + ('P' << 16) + ('D' << 8) + 'I') // "IDP3"
#define MD3_VERSION        15

/**
 * @brief MD3 file format limits.
 */
#define MD3_MAX_LODS       0x4 // per model
#define MD3_MAX_TRIANGLES  0x2000 // per mesh
#define MD3_MAX_VERTEXES   0x1000 // per mesh
#define MD3_MAX_SHADERS    0x100 // per mesh
#define MD3_MIN_FRAMES     0x1 // per model
#define MD3_MAX_FRAMES     0x1000 // per model
#define MD3_MAX_SURFACES   0x40 // per model
#define MD3_MAX_TAGS       0x10 // per frame
#define MD3_MAX_PATH       0x40 // relative file references
#define MD3_MAX_ANIMATIONS 0x20 // see EntityAnimation
#define MD3_XYZ_SCALE      (1.f / 64.f)

typedef struct {
  Vec2 st;
} Md3Texcoord;

typedef struct {
  Vec3s point;
  int16_t norm;
} Md3Vertex;

typedef struct {
  uint32_t indexes[3];
} Md3Triangle;

typedef struct {
  Box3 bounds;
  Vec3 translate;
  float radius;
  char name[16];
} Md3Frame;

typedef struct {
  char name[MD3_MAX_PATH];
  Vec3 origin;
  Vec3 axis[3];
} Md3Tag;

typedef struct {
  char name[MD3_MAX_PATH];
  int32_t index;
} Md3Shader;

typedef struct {
  int32_t id;

  char name[MD3_MAX_PATH];

  int32_t flags;

  int32_t num_frames;
  int32_t num_shaders;
  int32_t num_vertexes;
  int32_t num_triangles;

  int32_t ofs_triangles;
  int32_t ofs_shaders;
  int32_t ofs_texcoords;
  int32_t ofs_vertexes;
  int32_t ofs_end;
} Md3Surface;

typedef struct {
  int32_t id;
  int32_t version;

  char filename[MD3_MAX_PATH];

  int32_t flags;

  int32_t num_frames;
  int32_t num_tags;
  int32_t num_surfaces;
  int32_t num_shaders;

  int32_t ofs_frames;
  int32_t ofs_tags;
  int32_t ofs_surfaces;
  int32_t ofs_end;
} Md3;
