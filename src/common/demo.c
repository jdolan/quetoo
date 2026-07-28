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

#include "demo.h"
#include "mem.h"

// Portable little-endian field I/O. Demos are written little-endian regardless
// of host byte order, and contain no libnet-encoded fields at the container
// layer (record payloads are opaque), so this module depends only on common.

static bool Demo_WriteU8(file_t *file, uint8_t v) {
  return Fs_Write(file, &v, 1, 1) == 1;
}

static bool Demo_WriteU16(file_t *file, uint16_t v) {
  const byte b[2] = { (byte) v, (byte) (v >> 8) };
  return Fs_Write(file, b, 1, 2) == 2;
}

static bool Demo_WriteU32(file_t *file, uint32_t v) {
  const byte b[4] = { (byte) v, (byte) (v >> 8), (byte) (v >> 16), (byte) (v >> 24) };
  return Fs_Write(file, b, 1, 4) == 4;
}

static bool Demo_WriteU64(file_t *file, uint64_t v) {
  byte b[8];
  for (int32_t i = 0; i < 8; i++) {
    b[i] = (byte) (v >> (i * 8));
  }
  return Fs_Write(file, b, 1, 8) == 8;
}

static bool Demo_ReadU8(file_t *file, uint8_t *v) {
  return Fs_Read(file, v, 1, 1) == 1;
}

static bool Demo_ReadU16(file_t *file, uint16_t *v) {
  byte b[2];
  if (Fs_Read(file, b, 1, 2) != 2) {
    return false;
  }
  *v = (uint16_t) (b[0] | (b[1] << 8));
  return true;
}

static bool Demo_ReadU32(file_t *file, uint32_t *v) {
  byte b[4];
  if (Fs_Read(file, b, 1, 4) != 4) {
    return false;
  }
  *v = (uint32_t) b[0] | ((uint32_t) b[1] << 8) | ((uint32_t) b[2] << 16) | ((uint32_t) b[3] << 24);
  return true;
}

static bool Demo_ReadU64(file_t *file, uint64_t *v) {
  byte b[8];
  if (Fs_Read(file, b, 1, 8) != 8) {
    return false;
  }
  uint64_t r = 0;
  for (int32_t i = 0; i < 8; i++) {
    r |= (uint64_t) b[i] << (i * 8);
  }
  *v = r;
  return true;
}

/**
 * @return True if `file` begins with the v2 magic. Leaves the read cursor at 0.
 */
bool Demo_IsV2(file_t *file) {
  char magic[DEMO_MAGIC_LEN];

  if (!Fs_Seek(file, 0)) {
    return false;
  }

  const bool ok = Fs_Read(file, magic, 1, DEMO_MAGIC_LEN) == DEMO_MAGIC_LEN
    && memcmp(magic, DEMO_MAGIC, DEMO_MAGIC_LEN) == 0;

  Fs_Seek(file, 0);
  return ok;
}

/**
 * @brief Writes the magic, header, and epoch block to `file` (which must be empty).
 */
bool Demo_WriteHeader(file_t *file, const demo_header_t *header, const void *epoch, size_t epoch_len) {

  if (Fs_Write(file, DEMO_MAGIC, 1, DEMO_MAGIC_LEN) != DEMO_MAGIC_LEN) {
    return false;
  }

  if (!Demo_WriteU16(file, header->version) ||
    !Demo_WriteU16(file, header->flags) ||
    !Demo_WriteU16(file, header->protocol_major) ||
    !Demo_WriteU16(file, header->protocol_minor) ||
    !Demo_WriteU16(file, header->tick_rate) ||
    !Demo_WriteU32(file, (uint32_t) epoch_len)) {
    return false;
  }

  return epoch_len == 0 || Fs_Write(file, epoch, 1, epoch_len) == (int64_t) epoch_len;
}

/**
 * @brief Reads the header and epoch block. If `epoch` is non-NULL, `*epoch` is
 * Mem_Malloc'd and the caller must free it. The cursor is left at the first record.
 * @return False if the magic is absent (i.e. a legacy v1 demo).
 */
bool Demo_ReadHeader(file_t *file, demo_header_t *header, void **epoch) {
  char magic[DEMO_MAGIC_LEN];

  if (Fs_Read(file, magic, 1, DEMO_MAGIC_LEN) != DEMO_MAGIC_LEN ||
    memcmp(magic, DEMO_MAGIC, DEMO_MAGIC_LEN) != 0) {
    return false;
  }

  if (!Demo_ReadU16(file, &header->version) ||
    !Demo_ReadU16(file, &header->flags) ||
    !Demo_ReadU16(file, &header->protocol_major) ||
    !Demo_ReadU16(file, &header->protocol_minor) ||
    !Demo_ReadU16(file, &header->tick_rate) ||
    !Demo_ReadU32(file, &header->epoch_len)) {
    return false;
  }

  if (epoch) {
    *epoch = header->epoch_len ? Mem_Malloc(header->epoch_len) : NULL;
    if (header->epoch_len && Fs_Read(file, *epoch, 1, header->epoch_len) != (int64_t) header->epoch_len) {
      Mem_Free(*epoch);
      *epoch = NULL;
      return false;
    }
  } else if (header->epoch_len) {
    Fs_Seek(file, Fs_Tell(file) + (int64_t) header->epoch_len);
  }

  return true;
}

/**
 * @brief Writes one record (framing + payload) at the current cursor.
 */
bool Demo_WriteRecord(file_t *file, uint8_t type, uint32_t timecode, const void *payload, size_t length) {

  if (!Demo_WriteU8(file, type) ||
    !Demo_WriteU32(file, timecode) ||
    !Demo_WriteU32(file, (uint32_t) length)) {
    return false;
  }

  return length == 0 || Fs_Write(file, payload, 1, length) == (int64_t) length;
}

/**
 * @brief Reads the next record's framing into `record` and its payload into `buffer`.
 * @return False at end-of-stream (clean EOF) or if `buffer_size` is too small.
 */
bool Demo_ReadRecord(file_t *file, demo_record_t *record, void *buffer, size_t buffer_size) {

  if (!Demo_ReadU8(file, &record->type)) {
    return false; // clean EOF
  }

  if (!Demo_ReadU32(file, &record->timecode) ||
    !Demo_ReadU32(file, &record->length)) {
    return false;
  }

  if (record->length > buffer_size) {
    return false;
  }

  return record->length == 0 || Fs_Read(file, buffer, 1, record->length) == (int64_t) record->length;
}

/**
 * @brief Appends the seek-index footer (entries + count/duration/start + tail magic).
 */
bool Demo_WriteIndex(file_t *file, const demo_index_t *index) {

  const int64_t start = Fs_Tell(file);
  if (start < 0) {
    return false;
  }

  for (uint32_t i = 0; i < index->count; i++) {
    if (!Demo_WriteU32(file, index->entries[i].timecode) ||
      !Demo_WriteU64(file, index->entries[i].offset)) {
      return false;
    }
  }

  if (!Demo_WriteU32(file, index->count) ||
    !Demo_WriteU32(file, index->duration) ||
    !Demo_WriteU64(file, (uint64_t) start)) {
    return false;
  }

  return Fs_Write(file, DEMO_TAIL_MAGIC, 1, DEMO_TAIL_MAGIC_LEN) == DEMO_TAIL_MAGIC_LEN;
}

// Footer trailer = count(4) + duration(4) + start(8) + tail magic(4).
#define DEMO_FOOTER_TRAILER_LEN (4 + 4 + 8 + DEMO_TAIL_MAGIC_LEN)

/**
 * @brief Reads the index footer if present.
 * @return False if no footer is present (no tail magic).
 */
bool Demo_ReadIndex(file_t *file, demo_index_t *index) {

  const int64_t len = Fs_FileLength(file);
  if (len < (int64_t) DEMO_FOOTER_TRAILER_LEN) {
    return false;
  }

  char magic[DEMO_TAIL_MAGIC_LEN];
  if (!Fs_Seek(file, len - DEMO_TAIL_MAGIC_LEN) ||
    Fs_Read(file, magic, 1, DEMO_TAIL_MAGIC_LEN) != DEMO_TAIL_MAGIC_LEN ||
    memcmp(magic, DEMO_TAIL_MAGIC, DEMO_TAIL_MAGIC_LEN) != 0) {
    return false;
  }

  if (!Fs_Seek(file, len - (int64_t) DEMO_FOOTER_TRAILER_LEN)) {
    return false;
  }

  uint32_t count, duration;
  uint64_t start;
  if (!Demo_ReadU32(file, &count) ||
    !Demo_ReadU32(file, &duration) ||
    !Demo_ReadU64(file, &start)) {
    return false;
  }

  if (!Fs_Seek(file, (int64_t) start)) {
    return false;
  }

  index->count = count;
  index->duration = duration;
  index->entries = count ? Mem_Malloc(count * sizeof(demo_index_entry_t)) : NULL;

  for (uint32_t i = 0; i < count; i++) {
    if (!Demo_ReadU32(file, &index->entries[i].timecode) ||
      !Demo_ReadU64(file, &index->entries[i].offset)) {
      Demo_FreeIndex(index);
      return false;
    }
  }

  return true;
}

/**
 * @brief Rebuilds the index by scanning all records, for files that have no
 * footer (e.g. a recording interrupted by a crash). The cursor must be at the
 * first record on entry; it is left at the first record on return.
 */
bool Demo_ScanIndex(file_t *file, demo_index_t *index) {

  const int64_t first = Fs_Tell(file);
  if (first < 0) {
    return false;
  }

  // pass 1: count keyframes and find the final timecode
  uint32_t keyframes = 0, duration = 0;
  for (;;) {
    uint8_t type;
    uint32_t timecode, length;
    if (!Demo_ReadU8(file, &type)) {
      break; // clean EOF
    }
    if (!Demo_ReadU32(file, &timecode) || !Demo_ReadU32(file, &length)) {
      break;
    }
    if (type == DEMO_RECORD_FRAME_KEY) {
      keyframes++;
    }
    duration = timecode;
    if (!Fs_Seek(file, Fs_Tell(file) + (int64_t) length)) {
      break;
    }
  }

  index->count = keyframes;
  index->duration = duration;
  index->entries = keyframes ? Mem_Malloc(keyframes * sizeof(demo_index_entry_t)) : NULL;

  // pass 2: record the offset and timecode of each keyframe
  if (!Fs_Seek(file, first)) {
    Demo_FreeIndex(index);
    return false;
  }

  uint32_t i = 0;
  while (i < keyframes) {
    const int64_t offset = Fs_Tell(file);
    uint8_t type;
    uint32_t timecode, length;
    if (!Demo_ReadU8(file, &type) ||
      !Demo_ReadU32(file, &timecode) ||
      !Demo_ReadU32(file, &length)) {
      break;
    }
    if (type == DEMO_RECORD_FRAME_KEY) {
      index->entries[i].timecode = timecode;
      index->entries[i].offset = (uint64_t) offset;
      i++;
    }
    if (!Fs_Seek(file, Fs_Tell(file) + (int64_t) length)) {
      break;
    }
  }

  Fs_Seek(file, first);
  return true;
}

/**
 * @brief Frees an index's entries.
 */
void Demo_FreeIndex(demo_index_t *index) {

  if (index->entries) {
    Mem_Free(index->entries);
  }

  index->entries = NULL;
  index->count = 0;
}

/**
 * @return The latest index entry with `timecode <= t`, or NULL if none. Entries
 * are assumed to be in ascending timecode order (as written).
 */
const demo_index_entry_t *Demo_IndexFloor(const demo_index_t *index, uint32_t t) {

  const demo_index_entry_t *floor = NULL;

  for (uint32_t i = 0; i < index->count; i++) {
    if (index->entries[i].timecode <= t) {
      floor = &index->entries[i];
    } else {
      break;
    }
  }

  return floor;
}
