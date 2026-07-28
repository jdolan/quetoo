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

#include "filesystem.h"

/**
 * @brief The Quetoo demo v2 container format. A demo file is laid out as:
 *
 *   [magic "QDM2"][header][epoch block][record]*[optional index footer]
 *
 * Records are timecoded and self-framing. FRAME_KEY records are self-contained
 * full snapshots that seeking can jump to. Files lacking the magic are treated
 * as legacy v1 demos and played back by the original forward-only path.
 */

#define DEMO_MAGIC          "QDM2"
#define DEMO_MAGIC_LEN      4
#define DEMO_TAIL_MAGIC     "QDMX"
#define DEMO_TAIL_MAGIC_LEN 4
#define DEMO_FORMAT_VERSION 2

/**
 * @brief Keyframe cadence, in server ticks (~2s at QUETOO_TICK_RATE 40).
 */
#define DEMO_KEYFRAME_INTERVAL 80

/**
 * @brief Demo container flags (demo_header_t.flags).
 */
typedef enum {
  DEMO_FLAG_NONE      = 0,
  DEMO_FLAG_CLIENT    = 1 << 0, // recorded from a client POV (one player_state per frame)
  DEMO_FLAG_SERVER    = 1 << 1, // omniscient server recording (multi-POV)
  DEMO_FLAG_HAS_INDEX = 1 << 2, // a seek index footer is present
} demo_flags_t;

/**
 * @brief Demo record types (demo_record_t.type).
 */
typedef enum {
  DEMO_RECORD_FRAME_DELTA = 0, // one full frame, delta vs the previous frame
  DEMO_RECORD_FRAME_KEY   = 1, // self-contained full snapshot (seek target)
  DEMO_RECORD_RELIABLE    = 2, // out-of-band server command that survives seeks
  DEMO_RECORD_CAMERA      = 3, // reserved: embedded camera script (unused in v2.0)
} demo_record_type_t;

/**
 * @brief The demo file header, following the magic.
 */
typedef struct {
  uint16_t version;        // DEMO_FORMAT_VERSION
  uint16_t flags;          // demo_flags_t
  uint16_t protocol_major; // PROTOCOL_MAJOR at record time
  uint16_t protocol_minor; // PROTOCOL_MINOR at record time
  uint16_t tick_rate;      // QUETOO_TICK_RATE at record time
  uint32_t epoch_len;      // length of the epoch block that follows
} demo_header_t;

/**
 * @brief A single record's framing (the payload follows in the file).
 */
typedef struct {
  uint8_t  type;     // demo_record_type_t
  uint32_t timecode; // milliseconds from demo start
  uint32_t length;   // payload length in bytes
} demo_record_t;

/**
 * @brief One seek-index entry (one per FRAME_KEY record).
 */
typedef struct {
  uint32_t timecode; // ms
  uint64_t offset;   // byte offset of the FRAME_KEY record
} demo_index_entry_t;

/**
 * @brief An in-memory seek index.
 */
typedef struct {
  demo_index_entry_t *entries; // Mem_Malloc'd, count elements (NULL if count == 0)
  uint32_t count;
  uint32_t duration; // ms, of the whole demo
} demo_index_t;

bool Demo_IsV2(file_t *file);
bool Demo_WriteHeader(file_t *file, const demo_header_t *header, const void *epoch, size_t epoch_len);
bool Demo_ReadHeader(file_t *file, demo_header_t *header, void **epoch);
bool Demo_WriteRecord(file_t *file, uint8_t type, uint32_t timecode, const void *payload, size_t length);
bool Demo_ReadRecord(file_t *file, demo_record_t *record, void *buffer, size_t buffer_size);
bool Demo_WriteIndex(file_t *file, const demo_index_t *index);
bool Demo_ReadIndex(file_t *file, demo_index_t *index);
bool Demo_ScanIndex(file_t *file, demo_index_t *index);
void Demo_FreeIndex(demo_index_t *index);
const demo_index_entry_t *Demo_IndexFloor(const demo_index_t *index, uint32_t t);
