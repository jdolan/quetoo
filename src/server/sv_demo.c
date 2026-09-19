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
 * @brief Reads the keyframe index appended to the demo file.
 * @remarks `num_keyframes` is read straight off disk and sizes an allocation, so it is bounded
 * by what the file can actually hold before it is trusted. Anything else amiss - a header that
 * fails that bound, a table that cannot be seeked to, a short read - leaves the index empty
 * rather than rejecting the demo, which still plays perfectly well forward without one.
 * @remarks Entry offsets are deliberately not validated: a nonsensical one yields a garbage
 * chunk size, which `Sv_GetDemoMessage` already rejects.
 */
static void Sv_LoadDemoKeyframes(void) {

  sv.numDemoKeyframes = 0;

  const int64_t fileLength = Fs_FileLength(sv.demoFile);
  const int64_t ofs = sv.demoHeader.ofsKeyframes;

  int64_t maxKeyframes = 0;
  if (ofs >= 0 && fileLength > ofs) {
    maxKeyframes = (fileLength - ofs) / (int64_t) sizeof(DemoKeyframe);
  }

  if (sv.demoHeader.numKeyframes < 0 || sv.demoHeader.numKeyframes > maxKeyframes) {
    Com_Warn("%s: invalid num_keyframes %d\n", sv.name, sv.demoHeader.numKeyframes);
    return;
  }

  if (sv.demoHeader.numKeyframes == 0 || !Fs_Seek(sv.demoFile, ofs)) {
    return;
  }

  sv.numDemoKeyframes = sv.demoHeader.numKeyframes;
  sv.demoKeyframes = Mem_TagMalloc(sv.numDemoKeyframes * sizeof(DemoKeyframe), MEM_TAG_SERVER);

  for (int32_t i = 0; i < sv.numDemoKeyframes; i++) {

    DemoKeyframe entry;
    if (Fs_Read(sv.demoFile, &entry, sizeof(entry), 1) != 1) {
      sv.numDemoKeyframes = i;
      break;
    }

    entry.frameNum = LittleLong(entry.frameNum);
    entry.offset = LittleLong(entry.offset);

    sv.demoKeyframes[i] = entry;
  }
}

/**
 * @brief Opens the demo named by `sv.name` for playback, reading its header and keyframe index
 * and leaving the file positioned at the first recorded frame - not the setup chunks (server
 * data, config strings, baselines) ahead of it, which `Sv_SendDemoSetup` sends to each
 * connecting client individually rather than through the shared playback cursor. A demo that
 * fails to open or validate leaves `sv.demo_file` NULL, which `Sv_SendDemoPacket` treats as an
 * immediate end.
 */
void Sv_LoadDemo(void) {

  sv.demoFile = Fs_OpenRead(va("demos/%s.demo", sv.name));

  sv.demoPaused = false;

  if (!sv.demoFile) {
    return;
  }

  if (Fs_Read(sv.demoFile, &sv.demoHeader, sizeof(sv.demoHeader), 1) != 1 ||
      memcmp(sv.demoHeader.magic, DEMO_MAGIC, sizeof(sv.demoHeader.magic)) ||
      LittleLong(sv.demoHeader.version) != DEMO_VERSION) {

    Com_Warn("%s is not a valid demo file\n", sv.name);
    Fs_Close(sv.demoFile);
    sv.demoFile = NULL;
    return;
  }

  sv.demoHeader.duration = LittleLong(sv.demoHeader.duration);
  sv.demoHeader.numKeyframes = LittleLong(sv.demoHeader.numKeyframes);
  sv.demoHeader.ofsKeyframes = LittleLong(sv.demoHeader.ofsKeyframes);

  Sv_LoadDemoKeyframes();

  // a demo recorded with no frames at all has no keyframe to skip to; playback of it is moot
  // either way, since the very next read hits the terminator right behind the setup chunks
  if (sv.numDemoKeyframes > 0) {
    Fs_Seek(sv.demoFile, sv.demoKeyframes[0].offset);
  } else {
    Fs_Seek(sv.demoFile, sizeof(sv.demoHeader));
  }
}

/**
 * @brief Transmits the demo's setup chunks - server data, config strings, and baselines - to a
 * single connecting client, independent of the shared playback cursor `Sv_GetDemoFrame`
 * advances for everyone. Every recorded frame deltas against these same baselines, never
 * against another frame (see `Cl_WriteDemoMessage`), so this one-time catch-up is all a client
 * needs before it can start receiving whatever frame is currently being broadcast to everyone
 * else, no matter how far into the recording that already is.
 */
void Sv_SendDemoSetup(ServerClient *cl) {

  if (!sv.demoFile || sv.numDemoKeyframes == 0) {
    return;
  }

  const int64_t pos = Fs_Tell(sv.demoFile);
  const int64_t end = sv.demoKeyframes[0].offset;

  if (!Fs_Seek(sv.demoFile, sizeof(sv.demoHeader))) {
    Com_Warn("Failed to seek demo file\n");
    Fs_Seek(sv.demoFile, pos);
    return;
  }

  byte buffer[MAX_MSG_SIZE];

  while (Fs_Tell(sv.demoFile) < end) {

    int32_t size;
    if (Fs_Read(sv.demoFile, &size, sizeof(size), 1) != 1) {
      Com_Warn("Failed to read demo file\n");
      break;
    }

    size = LittleLong(size);
    if (size <= 0 || size > MAX_MSG_SIZE) {
      Com_Warn("Corrupt demo file: invalid chunk size %d\n", size);
      break;
    }

    int32_t frameNum;
    if (Fs_Read(sv.demoFile, &frameNum, sizeof(frameNum), 1) != 1) {
      Com_Warn("Incomplete or corrupt demo file\n");
      break;
    }

    if (Fs_Read(sv.demoFile, buffer, size, 1) != 1) {
      Com_Warn("Incomplete or corrupt demo file\n");
      break;
    }

    Netchan_Transmit(&cl->netChan, buffer, size);
  }

  // restore the shared playback cursor regardless of how the loop above ended, so a setup-read
  // failure can never leave every other client's ongoing broadcast reading from the wrong offset
  if (!Fs_Seek(sv.demoFile, pos)) {
    Com_Warn("Failed to restore demo file position\n");
  }
}

/**
 * @brief Releases the demo file and its keyframe index.
 */
void Sv_FreeDemo(void) {

  if (sv.demoFile) {
    Fs_Close(sv.demoFile);
  }

  Mem_Free(sv.demoKeyframes);
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

  ServerClient *cl = svs.clients;
  for (int32_t i = 0; i < sv_maxClients->integer; i++, cl++) {

    if (cl->state == SV_CLIENT_FREE) {
      continue;
    }

    Net_WriteByte(&cl->netChan.message, SV_CMD_DEMO_INFO);
    Net_WriteLong(&cl->netChan.message, sv.demoHeader.duration);
    Net_WriteByte(&cl->netChan.message, sv.demoPaused);
  }
}

/**
 * @brief Advances to the next demo in the playlist or restarts from the beginning.
 */
static void Sv_DemoCompleted(void) {

  if (sv_demoList->string[0]) {

    const char *currentDemo = sv.name;
    const char *nextDemo = q_strstr(sv_demoList->string, currentDemo);
    char demoToken[MAX_QPATH];

    if (!nextDemo) {

      nextDemo = sv_demoList->string;
    } else {

      nextDemo += q_strlen(currentDemo);

      if (nextDemo[0] == ' ') {
        nextDemo++;
      } else if (!nextDemo[0]) {
        nextDemo = sv_demoList->string;
      }
    }

    const char *space = q_strchr(nextDemo, ' ') ? : (nextDemo + q_strlen(nextDemo));
    size_t len = space - nextDemo;

    q_strlcpy(demoToken, nextDemo, len + 1);

    if (demoToken[0]) {
      Sv_InitServer(demoToken, NULL, SV_ACTIVE_DEMO);
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

  if (sv_demoList->string[0]) {
    Sv_DemoCompleted();
    return;
  }

  sv.demoPaused = true;
  Sv_SendDemoInfo();
}

/**
 * @brief Reads the next frame from the current demo file into the specified buffer,
 * returning the size of the frame in bytes. Each demo message is prefixed by its length
 * and the frame number it was recorded at; `frame_num`, if non-NULL, receives the latter.
 */
static size_t Sv_GetDemoMessage(byte *buffer, int32_t *frameNum) {
  int32_t size;
  int32_t num;
  int64_t r;

  if (!sv.demoFile) { // failed to open, or failed header validation
    Sv_DemoCompleted();
    return 0;
  }

  r = Fs_Read(sv.demoFile, &size, sizeof(size), 1);

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

  r = Fs_Read(sv.demoFile, &num, sizeof(num), 1);

  if (r != 1) {
    Com_Warn("Incomplete or corrupt demo file\n");
    Sv_DemoCompleted();
    return 0;
  }

  num = LittleLong(num);
  sv.demoFrameNum = num;
  if (frameNum) {
    *frameNum = num;
  }

  r = Fs_Read(sv.demoFile, buffer, size, 1);

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

  if (svs.state != SV_ACTIVE_DEMO || !sv.demoFile || sv.numDemoKeyframes == 0) {
    return;
  }

  const int32_t targetFrame = millis / QUETOO_TICK_MILLIS;

  int32_t lo = 0, hi = sv.numDemoKeyframes - 1, best = 0;
  while (lo <= hi) {
    const int32_t mid = (lo + hi) / 2;
    if (sv.demoKeyframes[mid].frameNum <= targetFrame) {
      best = mid;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }

  if (!Fs_Seek(sv.demoFile, sv.demoKeyframes[best].offset)) {
    Com_Warn("Failed to seek demo file\n");
    return;
  }

  // only while paused: playback that is running reaches the seek destination by itself, and an
  // unconsumed flag would release an extra frame at whatever point it is next paused
  sv.demoStep = sv.demoPaused;
}

/**
 * @brief Reads this tick's demo frame once, so every connected client can be transmitted the
 * same bytes, rather than each client consuming its own chunk from the shared demo file.
 * @return The size of the frame in `buffer`, or 0 if none is due this tick: playback is paused
 * with no pending seek, or the recording just ended.
 */
size_t Sv_GetDemoFrame(byte *buffer) {

  if (sv.demoPaused) {

    // a seek taken while paused still has to show where it landed, or the transport controls
    // appear dead: scrubbing and the rewind/forward buttons would move the read position
    // silently, and playback would later resume from somewhere the viewer never chose
    if (!sv.demoStep) {
      return 0;
    }

    sv.demoStep = false;
  }

  return Sv_GetDemoMessage(buffer, NULL);
}

/**
 * @brief Transmits this tick's demo frame, read once by `Sv_GetDemoFrame` and shared by every
 * client, to the given client.
 * @return False once the recording is exhausted, ending the send loop for this tick.
 */
bool Sv_SendDemoPacket(ServerClient *cl, byte *buffer, size_t size) {

  if (sv.demoPaused) {

    if (size) {
      Netchan_Transmit(&cl->netChan, buffer, size);
      return true;
    }

    // otherwise send no frame, but still flush pending reliable data (Sv_SendDemoInfo's pause
    // state, which the transport controls are waiting on) and keep the netchan alive: the client
    // applies its normal timeout check regardless of demo state, and would otherwise disconnect
    // a spectator who paused playback for longer than cl_timeout
    if (cl->netChan.message.size || quetoo.ticks - cl->netChan.lastSent > 1000) {
      Netchan_Transmit(&cl->netChan, NULL, 0);
    }

    return true;
  }

  if (!size) {
    return false;
  }

  Netchan_Transmit(&cl->netChan, buffer, size);

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
  const int32_t current = sv.demoFrameNum * QUETOO_TICK_MILLIS;

  Sv_SeekDemo(Maxi(0, current + delta));
}

/**
 * @brief Toggles demo playback pause. No-op outside of demo playback.
 */
void Sv_DemoPause_f(void) {

  if (svs.state != SV_ACTIVE_DEMO) {
    return;
  }

  sv.demoPaused = !sv.demoPaused;

  Sv_SendDemoInfo();
}
