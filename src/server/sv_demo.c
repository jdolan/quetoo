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

#include "sv_local.h"

/**
 * @brief Reads and validates the keyframe index appended to the demo file, leaving
 * `sv.num_demo_keyframes` at 0 if it cannot be trusted.
 */
static void Sv_LoadDemoKeyframes(void) {

  sv.num_demo_keyframes = sv.demo_header.keyframe_count;

  // a corrupt or malicious header can claim an arbitrary (including negative) keyframe_count
  // or keyframe_table_offset; bound both before trusting either for an allocation or a
  // pointer-arithmetic-like subtraction. A negative offset would otherwise make
  // file_length - offset artificially huge, defeating the capacity clamp entirely.
  const int64_t file_length = Fs_FileLength(sv.demo_file);

  int64_t max_keyframes = 0;
  if (sv.demo_header.keyframe_table_offset >= 0 && file_length > sv.demo_header.keyframe_table_offset) {
    max_keyframes = (file_length - sv.demo_header.keyframe_table_offset) / (int64_t) sizeof(demo_keyframe_t);
  }

  if (sv.num_demo_keyframes < 0 || sv.num_demo_keyframes > max_keyframes) {
    Com_Warn("%s: invalid keyframe_count %d, clamping to %" PRId64 "\n",
              sv.name, sv.num_demo_keyframes, max_keyframes);
    sv.num_demo_keyframes = (int32_t) max_keyframes;
  }

  if (sv.num_demo_keyframes == 0) {
    return;
  }

  if (!Fs_Seek(sv.demo_file, sv.demo_header.keyframe_table_offset)) {
    // couldn't seek to the keyframe table (corrupt or truncated file): leave sv.demo_keyframes
    // NULL, but sv.num_demo_keyframes must track it, or Sv_SeekDemo's `num_demo_keyframes == 0`
    // guard is bypassed and it dereferences a NULL table
    sv.num_demo_keyframes = 0;
    return;
  }

  sv.demo_keyframes = Mem_TagMalloc(sv.num_demo_keyframes * sizeof(demo_keyframe_t), MEM_TAG_SERVER);

  for (int32_t i = 0; i < sv.num_demo_keyframes; i++) {
    demo_keyframe_t entry;
    if (Fs_Read(sv.demo_file, &entry, sizeof(entry), 1) != 1) {
      sv.num_demo_keyframes = i;
      break;
    }
    entry.frame_num = LittleLong(entry.frame_num);
    entry.offset = LittleLong(entry.offset);

    // a corrupt table could point a "seek here" offset into the header region or past the table
    // itself; Sv_GetDemoMessage's own size validation would still catch the resulting garbage
    // read as a corrupt chunk, but there's no reason to accept an entry that's already nonsensical
    if (entry.offset < (int32_t) sizeof(sv.demo_header) ||
        entry.offset >= sv.demo_header.keyframe_table_offset) {
      Com_Warn("%s: keyframe %d has out-of-range offset %d, truncating table\n",
                sv.name, i, entry.offset);
      sv.num_demo_keyframes = i;
      break;
    }

    sv.demo_keyframes[i] = entry;
  }
}

/**
 * @brief Opens the demo named by `sv.name` for playback, reading its header and keyframe index
 * and leaving the file positioned at the first recorded message. A demo that fails to open or
 * validate leaves `sv.demo_file` NULL, which `Sv_SendDemoPacket` treats as an immediate end.
 */
void Sv_LoadDemo(void) {

  sv.demo_file = Fs_OpenRead(va("demos/%s.demo", sv.name));

  // interactive playback opens paused on the first frame, so the transport controls are up
  // and the viewer decides when to start; a playlist-driven demo server just plays
  sv.demo_paused = !sv_demo_list->string[0];
  sv.demo_step = sv.demo_paused;

  if (!sv.demo_file) {
    return;
  }

  if (Fs_Read(sv.demo_file, &sv.demo_header, sizeof(sv.demo_header), 1) != 1 ||
      memcmp(sv.demo_header.magic, DEMO_MAGIC, sizeof(sv.demo_header.magic)) ||
      LittleLong(sv.demo_header.version) != DEMO_VERSION) {

    Com_Warn("%s is not a valid demo file\n", sv.name);
    Fs_Close(sv.demo_file);
    sv.demo_file = NULL;
    return;
  }

  sv.demo_header.duration = LittleLong(sv.demo_header.duration);
  sv.demo_header.keyframe_count = LittleLong(sv.demo_header.keyframe_count);
  sv.demo_header.keyframe_table_offset = LittleLong(sv.demo_header.keyframe_table_offset);

  Sv_LoadDemoKeyframes();

  Fs_Seek(sv.demo_file, sizeof(sv.demo_header));
}

/**
 * @brief Releases the demo file and its keyframe index.
 */
void Sv_FreeDemo(void) {

  if (sv.demo_file) {
    Fs_Close(sv.demo_file);
  }

  Mem_Free(sv.demo_keyframes);
}

/**
 * @brief Publishes demo duration and pause state to every connected client. Pause is server
 * authoritative: the client mirrors it to gate the transport controls, the mouse grab and UI
 * event routing, and would otherwise have no way to learn about a pause it did not ask for,
 * such as reaching the end of the recording.
 */
void Sv_SendDemoInfo(void) {

  if (svs.state != SV_ACTIVE_DEMO) {
    return;
  }

  sv_client_t *cl = svs.clients;
  for (int32_t i = 0; i < sv_max_clients->integer; i++, cl++) {

    if (cl->state == SV_CLIENT_FREE) {
      continue;
    }

    Net_WriteByte(&cl->net_chan.message, SV_CMD_DEMO_INFO);
    Net_WriteLong(&cl->net_chan.message, sv.demo_header.duration);
    Net_WriteByte(&cl->net_chan.message, sv.demo_paused);
  }
}

/**
 * @brief Advances to the next demo in the playlist or restarts from the beginning.
 */
static void Sv_DemoCompleted(void) {

  if (sv_demo_list->string[0]) {

    const char *current_demo = sv.name;
    const char *next_demo = q_strstr(sv_demo_list->string, current_demo);
    char demo_token[MAX_QPATH];

    if (!next_demo) {

      next_demo = sv_demo_list->string;
    } else {

      next_demo += q_strlen(current_demo);

      if (next_demo[0] == ' ') {
        next_demo++;
      } else if (!next_demo[0]) {
        next_demo = sv_demo_list->string;
      }
    }

    const char *space = q_strchr(next_demo, ' ') ? : (next_demo + q_strlen(next_demo));
    size_t len = space - next_demo;

    q_strlcpy(demo_token, next_demo, len + 1);

    if (demo_token[0]) {
      Sv_InitServer(demo_token, NULL, SV_ACTIVE_DEMO);
    } else {
      Sv_ShutdownServer("Demo complete\n");
    }
  } else {
    Sv_ShutdownServer("Demo complete\n");
  }
}

/**
 * @brief Handles reaching the end of the recording. Interactive playback holds on the last frame,
 * paused, so the viewer can scrub back and watch it again rather than being dropped to the menus;
 * a playlist-driven demo server still moves on to the next demo.
 */
static void Sv_DemoEnded(void) {

  if (sv_demo_list->string[0]) {
    Sv_DemoCompleted();
    return;
  }

  sv.demo_paused = true;
  Sv_SendDemoInfo();
}

/**
 * @brief Reads the next frame from the current demo file into the specified buffer,
 * returning the size of the frame in bytes. Each demo message is prefixed by its length
 * and the frame number it was recorded at; `frame_num`, if non-NULL, receives the latter.
 */
static size_t Sv_GetDemoMessage(byte *buffer, int32_t *frame_num) {
  int32_t size;
  int32_t num;
  int64_t r;

  if (!sv.demo_file) { // failed to open, or failed header validation
    Sv_DemoCompleted();
    return 0;
  }

  r = Fs_Read(sv.demo_file, &size, sizeof(size), 1);

  if (r != 1) { // improperly terminated demo file; treat a truncated recording as an end
    Com_Warn("Failed to read demo file\n");
    Sv_DemoEnded();
    return 0;
  }

  size = LittleLong(size);

  if (size == -1) { // properly terminated demo file
    Sv_DemoEnded();
    return 0;
  }

  // Fs_Read divides its byte count by `size`, so 0 is a divide-by-zero; any other non-positive
  // value implicitly converts to an enormous size_t read count when passed to Fs_Read below.
  // Only -1 (already handled above) is a legitimate non-positive value here.
  if (size <= 0 || size > MAX_MSG_SIZE) { // corrupt demo file
    Com_Warn("Corrupt demo file: invalid chunk size %d\n", size);
    Sv_DemoCompleted();
    return 0;
  }

  r = Fs_Read(sv.demo_file, &num, sizeof(num), 1);

  if (r != 1) {
    Com_Warn("Incomplete or corrupt demo file\n");
    Sv_DemoCompleted();
    return 0;
  }

  num = LittleLong(num);
  sv.demo_frame_num = num;
  if (frame_num) {
    *frame_num = num;
  }

  r = Fs_Read(sv.demo_file, buffer, size, 1);

  if (r != 1) {
    Com_Warn("Incomplete or corrupt demo file\n");
    Sv_DemoCompleted();
    return 0;
  }

  return size;
}

/**
 * @brief Seeks demo playback directly to the frame nearest and at-or-before the target time.
 * @details Every recorded frame is fully self-contained - delta-encoded against the demo's
 * baselines and a null player state, never against another frame (see `Cl_WriteDemoMessage`) -
 * so any indexed offset is always a safe, independent jump target: there is no baseline chain or
 * prior-frame dependency to reconstruct. This binary-searches `sv.demo_keyframes` (one entry per
 * recorded frame) and seeks the file there; normal per-tick sending in `Sv_SendDemoPacket`
 * picks up again from that point with no special catch-up pacing required.
 */
void Sv_SeekDemo(int32_t millis) {

  if (svs.state != SV_ACTIVE_DEMO || !sv.demo_file || sv.num_demo_keyframes == 0) {
    return;
  }

  const int32_t target_frame = millis / QUETOO_TICK_MILLIS;

  int32_t lo = 0, hi = sv.num_demo_keyframes - 1, best = 0;
  while (lo <= hi) {
    const int32_t mid = (lo + hi) / 2;
    if (sv.demo_keyframes[mid].frame_num <= target_frame) {
      best = mid;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }

  if (!Fs_Seek(sv.demo_file, sv.demo_keyframes[best].offset)) {
    Com_Warn("Failed to seek demo file\n");
    return;
  }

  sv.demo_step = true;
}

/**
 * @brief Transmits this tick's demo frame to the given client.
 * @return False once the recording is exhausted, ending the send loop for this tick.
 */
bool Sv_SendDemoPacket(sv_client_t *cl) {
  byte buffer[MAX_MSG_SIZE];
  size_t size;

  if (sv.demo_paused) {

    // a seek taken while paused still has to show where it landed, or the transport controls
    // appear dead: scrubbing and the rewind/forward buttons would move the read position
    // silently, and playback would later resume from somewhere the viewer never chose
    if (sv.demo_step) {
      sv.demo_step = false;

      if ((size = Sv_GetDemoMessage(buffer, NULL))) {
        Netchan_Transmit(&cl->net_chan, buffer, size);
        return true;
      }
    }

    // otherwise send no frame, but still flush pending reliable data (Sv_SendDemoInfo's pause
    // state, which the transport controls are waiting on) and keep the netchan alive: the client
    // applies its normal timeout check regardless of demo state, and would otherwise disconnect
    // a spectator who paused playback for longer than cl_timeout
    if (cl->net_chan.message.size || quetoo.ticks - cl->net_chan.last_sent > 1000) {
      Netchan_Transmit(&cl->net_chan, NULL, 0);
    }

    return true;
  }

  if (!(size = Sv_GetDemoMessage(buffer, NULL))) {
    return false;
  }

  Netchan_Transmit(&cl->net_chan, buffer, size);

  return true;
}

/**
 * @brief Seeks demo playback to the millisecond offset given by the connected spectator,
 * e.g. from a scrubber control in the UI. No-op outside of demo playback.
 */
void Sv_DemoSeek_f(void) {

  if (svs.state != SV_ACTIVE_DEMO) {
    return;
  }

  if (Cmd_Argc() != 2) {
    return;
  }

  Sv_SeekDemo((int32_t) strtol(Cmd_Argv(1), NULL, 10));
}

/**
 * @brief Seeks demo playback by the given millisecond offset, relative to the current
 * position, e.g. from a rewind/fast-forward keybind. No-op outside of demo playback.
 */
void Sv_DemoSeekRelative_f(void) {

  if (svs.state != SV_ACTIVE_DEMO) {
    return;
  }

  if (Cmd_Argc() != 2) {
    return;
  }

  const int32_t delta = (int32_t) strtol(Cmd_Argv(1), NULL, 10);
  const int32_t current = sv.demo_frame_num * QUETOO_TICK_MILLIS;

  Sv_SeekDemo(Maxi(0, current + delta));
}

/**
 * @brief Toggles demo playback pause. No-op outside of demo playback.
 */
void Sv_DemoPause_f(void) {

  if (svs.state != SV_ACTIVE_DEMO) {
    return;
  }

  sv.demo_paused = !sv.demo_paused;

  Sv_SendDemoInfo();
}
