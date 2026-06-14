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
#include "common/demo.h"

/**
 * @brief Server-side demo recording (serverrecord). The server captures an
 * omniscient snapshot each frame -- every visible entity, with no per-client
 * PVS limiting -- and writes it as a v2 demo that plays back through the same
 * server-hosted path as a client demo. The default POV follows the first
 * in-game client; the viewer can detach the free camera at any time.
 */

/**
 * @brief Ping-pong omniscient frame buffers for delta encoding the recording.
 * Held here rather than in sv_server_t because sv_client_frame_t is defined
 * after sv_server_t in sv_types.h.
 */
static sv_client_frame_t sv_record_frames[2];

/**
 * @brief Index of the current record frame buffer (0 or 1).
 */
static int32_t sv_record_frame;

/**
 * @brief Flushes an accumulated startup sub-message as a RELIABLE record at
 * timecode 0, mirroring the client recorder's epoch.
 */
static void Sv_FlushRecordHeader(mem_buf_t *msg) {

  if (msg->size == 0) {
    return;
  }

  Demo_WriteRecord(sv.record_file, DEMO_RECORD_RELIABLE, 0, msg->data, msg->size);
  msg->size = 0;
}

/**
 * @brief Writes the v2 header and epoch (server data, config strings, entity
 * baselines) so the demo is self-contained.
 */
static void Sv_WriteRecordEpoch(void) {
  static entity_state_t null_state;
  mem_buf_t msg;
  byte buffer[MAX_MSG_SIZE];

  const demo_header_t header = {
    .version = DEMO_FORMAT_VERSION,
    .flags = DEMO_FLAG_SERVER,
    .protocol_major = PROTOCOL_MAJOR,
    .protocol_minor = (uint16_t) svs.game->protocol,
    .tick_rate = QUETOO_TICK_RATE,
    .epoch_len = 0,
  };
  Demo_WriteHeader(sv.record_file, &header, NULL, 0);

  Mem_InitBuffer(&msg, buffer, sizeof(buffer));

  Net_WriteByte(&msg, SV_CMD_SERVER_DATA);
  Net_WriteLong(&msg, PROTOCOL_MAJOR);
  Net_WriteLong(&msg, svs.game->protocol);
  Net_WriteByte(&msg, 1); // demo_server byte
  Net_WriteString(&msg, Cvar_GetString("game"));
  Net_WriteString(&msg, sv.config_strings[CS_MESSAGE]);

  for (int32_t i = 0; i < MAX_CONFIG_STRINGS; i++) {
    if (sv.config_strings[i][0] != '\0') {
      if (msg.size + strlen(sv.config_strings[i]) + 32 > msg.max_size) {
        Sv_FlushRecordHeader(&msg);
      }
      Net_WriteByte(&msg, SV_CMD_CONFIG_STRING);
      Net_WriteShort(&msg, i);
      Net_WriteString(&msg, sv.config_strings[i]);
    }
  }

  for (int32_t i = 0; i < sv_max_entities->integer; i++) {
    const entity_state_t *baseline = &sv.entities[i].baseline;
    if (i != 0 && !baseline->number) { // entity 0 is worldspawn; never skip it
      continue;
    }
    if (msg.size + 64 > msg.max_size) {
      Sv_FlushRecordHeader(&msg);
    }
    Net_WriteByte(&msg, SV_CMD_BASELINE);
    Net_WriteDeltaEntity(&msg, &null_state, baseline, true);
  }

  Net_WriteByte(&msg, SV_CMD_CBUF_TEXT);
  Net_WriteString(&msg, "precache 0\n");

  Sv_FlushRecordHeader(&msg);
}

/**
 * @brief Builds an omniscient snapshot of all visible entities and a default
 * player state (the first in-game client, else a frozen spectator).
 */
static void Sv_BuildRecordFrame(sv_client_frame_t *frame) {

  frame->entity_state = svs.next_entity_state;
  frame->num_entities = 0;

  // default POV: the first active, non-bot client; otherwise a frozen spectator
  const sv_client_t *cl = svs.clients;
  const g_client_t *pov = NULL;
  for (int32_t i = 0; i < sv_max_clients->integer; i++, cl++) {
    if (cl->state == SV_CLIENT_ACTIVE && cl->gclient->in_use && !cl->gclient->ai) {
      pov = cl->gclient;
      break;
    }
  }

  if (pov) {
    frame->ps = pov->ps;
  } else {
    memset(&frame->ps, 0, sizeof(frame->ps));
    frame->ps.pm_state.type = PM_FREEZE;
  }

  // every visible entity, with no per-client PVS limiting
  for (int32_t i = 0; i < sv_max_entities->integer; i++) {

    const g_entity_t *ent = sv.entities[i].gent;

    if (!ent->in_use) {
      continue;
    }

    if (!ent->s.event && !ent->s.effects && !ent->s.trail && !ent->s.model1 && !ent->s.sound) {
      continue;
    }

    entity_state_t *s = &svs.entity_states[svs.next_entity_state % svs.num_entity_states];
    *s = ent->s;

    svs.next_entity_state++;
    frame->num_entities++;
  }
}

/**
 * @brief Builds and writes one recorded frame as a v2 FRAME record.
 */
static void Sv_WriteRecordFrame(uint32_t timecode, bool keyframe) {

  sv_client_frame_t *frame = &sv_record_frames[sv_record_frame];
  sv_client_frame_t *delta = keyframe ? NULL : &sv_record_frames[1 - sv_record_frame];

  Sv_BuildRecordFrame(frame);

  mem_buf_t msg;
  byte buffer[MAX_MSG_SIZE];
  Mem_InitBuffer(&msg, buffer, sizeof(buffer));
  msg.allow_overflow = true;

  Net_WriteByte(&msg, SV_CMD_FRAME);
  Net_WriteLong(&msg, sv.frame_num);
  Net_WriteLong(&msg, keyframe ? -1 : sv.record_last_frame);

  Sv_WritePlayerState(delta, frame, &msg);
  Sv_WriteEntities(delta, frame, &msg);

  if (!msg.overflowed) {
    Demo_WriteRecord(sv.record_file, keyframe ? DEMO_RECORD_FRAME_KEY : DEMO_RECORD_FRAME_DELTA,
                     timecode, msg.data, msg.size);
  }

  sv.record_last_frame = sv.frame_num;
  sv_record_frame = 1 - sv_record_frame;
}

/**
 * @brief Records one frame, if a server demo is being recorded. Called each
 * server tick after the game frame has run.
 */
void Sv_DemoRecordFrame(void) {

  if (!sv.recording || svs.state != SV_ACTIVE_GAME) {
    return;
  }

  if (sv.record_first_frame < 0) {
    sv.record_first_frame = sv.frame_num;
  }

  const int32_t rel_frame = sv.frame_num - sv.record_first_frame;
  const uint32_t timecode = (uint32_t) rel_frame * QUETOO_TICK_MILLIS;
  const bool keyframe = (rel_frame == 0) || (rel_frame % DEMO_KEYFRAME_INTERVAL) == 0;

  Sv_WriteRecordFrame(timecode, keyframe);
}

/**
 * @brief Stops any in-progress server recording and closes the file.
 */
void Sv_StopServerRecord(void) {

  if (!sv.recording) {
    return;
  }

  if (sv.record_file) {
    Fs_Close(sv.record_file);
    sv.record_file = NULL;
  }

  sv.recording = false;
}

/**
 * @brief sv_record [name] — begins recording an omniscient server demo.
 */
void Sv_Record_f(void) {

  if (svs.state != SV_ACTIVE_GAME) {
    Com_Print("You must be running a game to record\n");
    return;
  }

  if (sv.recording) {
    Com_Print("Already recording\n");
    return;
  }

  char name[MAX_QPATH];
  if (Cmd_Argc() == 2) {
    g_snprintf(name, sizeof(name), "demos/%s.demo", Cmd_Argv(1));
  } else {
    const time_t t = time(NULL);
    const struct tm *tm = localtime(&t);
    char datestamp[32];
    strftime(datestamp, sizeof(datestamp), "%Y-%m-%d-%H-%M-%S", tm);
    g_snprintf(name, sizeof(name), "demos/server-%s.demo", datestamp);
  }

  if (!(sv.record_file = Fs_OpenWrite(name))) {
    Com_Warn("Couldn't open %s\n", name);
    return;
  }

  sv.recording = true;
  sv.record_first_frame = -1;
  sv.record_last_frame = -1;
  sv_record_frame = 0;

  Sv_WriteRecordEpoch();

  Com_Print("Recording server demo to %s\n", name);
}

/**
 * @brief sv_stoprecord — stops recording a server demo.
 */
void Sv_StopRecord_f(void) {

  if (!sv.recording) {
    Com_Print("Not recording a server demo\n");
    return;
  }

  Sv_StopServerRecord();
  Com_Print("Stopped server demo\n");
}

/**
 * @brief Demo seek re-encoding (playback side).
 *
 * Client demos are captured verbatim: every recorded frame is delta-compressed
 * against whatever frame the client had acknowledged at record time. That chain
 * is intact for forward playback but breaks the instant a seek skips frames --
 * the client rejects a delta whose base it never received ("Delta frame too
 * old"). To make scrubbing safe on *any* demo, the whole demo is decoded once at
 * load time and re-emitted as a clean, self-consistent stream: a real (full)
 * keyframe every DEMO_KEYFRAME_INTERVAL frames, and every other frame delta'd
 * against the immediately preceding frame. Seeking then jumps to a keyframe and
 * the existing playback path streams the intervening deltas in order, so the
 * delta chain is never crossed and interpolation (which only happens on delta
 * frames) stays smooth.
 */

/**
 * @brief A fully-decoded snapshot: the player state plus every live entity,
 * indexed by entity number.
 */
typedef struct {
  player_state_t ps;
  entity_state_t entities[MAX_ENTITIES];
  bool active[MAX_ENTITIES];
  int32_t frame_num; // the original frame this snapshot holds, or -1 if empty
} sv_demo_snapshot_t;

/**
 * @brief Running decoder state for a re-encode pass.
 *
 * Recorded frames do not always delta against the immediately preceding frame --
 * the live server deltas against whatever frame the client last acknowledged,
 * which lags. So, exactly like the client (cl.frames[]), decoded frames are kept
 * in a ring indexed by frame_num & PACKET_MASK and each frame is decoded against
 * its true delta base, however many frames back that is.
 */
typedef struct {
  sv_demo_snapshot_t ring[PACKET_BACKUP];
  entity_state_t baselines[MAX_ENTITIES];
} sv_demo_decoder_t;

/**
 * @brief Scratch file the re-encoded, seekable stream is written to. A single
 * demo plays at a time, so one fixed path is reused and deleted on unload. The
 * leading dot and ".tmp" suffix keep it out of the demo browser listing.
 */
#define SV_DEMO_SEEK_TEMP "demos/.seek.tmp"

/**
 * @brief Walks an epoch (startup) RELIABLE record, capturing entity baselines so
 * later frames can be decoded and re-encoded against them. The record holds only
 * server-data, config-string, baseline, and cbuf-text commands; anything else
 * means the stream is not one we know how to decode.
 */
static bool Sv_DemoParseEpoch(sv_demo_decoder_t *dec, byte *data, uint32_t len) {
  static entity_state_t null_state;
  mem_buf_t in;

  Mem_InitBuffer(&in, data, len);
  in.size = len;

  while (in.read < in.size) {
    const byte cmd = Net_ReadByte(&in);

    switch (cmd) {
      case SV_CMD_SERVER_DATA:
        Net_ReadLong(&in);   // protocol major
        Net_ReadLong(&in);   // protocol minor
        Net_ReadByte(&in);   // demo_server byte
        Net_ReadString(&in); // game
        Net_ReadString(&in); // level message
        break;
      case SV_CMD_CONFIG_STRING:
        Net_ReadShort(&in);  // index
        Net_ReadString(&in); // value
        break;
      case SV_CMD_BASELINE: {
        const int16_t number = Net_ReadShort(&in);
        const uint16_t bits = Net_ReadShort(&in);
        if (number < 0 || number >= MAX_ENTITIES) {
          Com_Warn("Demo re-encode: bad baseline number %d\n", number);
          return false;
        }
        Net_ReadDeltaEntity(&in, &null_state, &dec->baselines[number], number, bits);
        break;
      }
      case SV_CMD_CBUF_TEXT:
        Net_ReadString(&in);
        break;
      default:
        // a non-startup command (e.g. a cgame command in a frame's reliable
        // prefix). Baselines only ever appear in the pure-engine startup epoch,
        // ahead of any such command, so stop here with what we have -- we cannot
        // know this command's length, but we do not need to: the record is
        // copied through verbatim regardless.
        return true;
    }

    if (in.read > in.size) {
      Com_Warn("Demo re-encode: epoch record overran\n");
      return false;
    }
  }

  return true;
}

/**
 * @brief Locates and decodes the SV_CMD_FRAME within a recorded FRAME record,
 * advancing the running state from the previous frame to this one.
 *
 * Demos recorded once frames were isolated (see Cl_WriteDemoRecord) have the
 * frame command at offset 0. Older demos may carry the reliable commands the
 * netchan prepended ahead of it; those engine commands are skipped here (and any
 * baselines among them captured). A cgame command cannot be skipped -- only the
 * client game module knows its length -- so the re-encode bails if one precedes
 * the frame, and that demo falls back to plain forward playback.
 *
 * The frame decode mirrors the client's parser exactly (Cl_ParseFrame /
 * Cl_ParseEntities): existing entities delta from their delta-base state, new
 * entities from their baseline, U_REMOVE drops them. The decoded frame is stored
 * in the ring so it can serve as a delta base for later frames.
 *
 * @param out_start Set to the byte offset of the SV_CMD_FRAME command.
 * @param out_end Set to the byte offset just past the frame command (anything
 * beyond is trailing datagram, e.g. sounds and temp entities).
 * @param out_frame_num Set to the decoded frame's original frame number.
 * @return True on success.
 */
static bool Sv_DemoDecodeFrame(sv_demo_decoder_t *dec, byte *data, uint32_t len,
                               uint32_t *out_start, uint32_t *out_end, int32_t *out_frame_num) {
  static entity_state_t null_es;
  static player_state_t null_ps;
  mem_buf_t in;

  Mem_InitBuffer(&in, data, len);
  in.size = len;

  // walk to the frame command, skipping any engine commands ahead of it
  for (;;) {
    if (in.read >= in.size) {
      Com_Warn("Demo re-encode: no frame command in record\n");
      return false;
    }

    const uint32_t cmd_pos = (uint32_t) in.read;
    const byte cmd = Net_ReadByte(&in);

    if (cmd == SV_CMD_FRAME) {
      *out_start = cmd_pos;
      break;
    }

    switch (cmd) {
      case SV_CMD_CONFIG_STRING:
        Net_ReadShort(&in);
        Net_ReadString(&in);
        break;
      case SV_CMD_PRINT:
        Net_ReadByte(&in);
        Net_ReadString(&in);
        break;
      case SV_CMD_CBUF_TEXT:
        Net_ReadString(&in);
        break;
      case SV_CMD_BASELINE: {
        const int16_t number = Net_ReadShort(&in);
        const uint16_t bits = Net_ReadShort(&in);
        if (number >= 0 && number < MAX_ENTITIES) {
          Net_ReadDeltaEntity(&in, &null_es, &dec->baselines[number], number, bits);
        }
        break;
      }
      case SV_CMD_SERVER_DATA:
        Net_ReadLong(&in);
        Net_ReadLong(&in);
        Net_ReadByte(&in);
        Net_ReadString(&in);
        Net_ReadString(&in);
        break;
      case SV_CMD_DISCONNECT:
      case SV_CMD_RECONNECT:
      case SV_CMD_DROP:
        break; // no payload
      default:
        Com_Warn("Demo re-encode: cannot skip command %u before frame\n", cmd);
        return false;
    }

    if (in.read > in.size) {
      Com_Warn("Demo re-encode: command overran record\n");
      return false;
    }
  }

  const int32_t frame_num = Net_ReadLong(&in); // original frame number
  const int32_t delta = Net_ReadLong(&in);     // original delta frame number (< 0 if uncompressed)

  sv_demo_snapshot_t *cur = &dec->ring[frame_num & PACKET_MASK];

  // seed this frame from its true delta base, then apply the recorded delta
  const player_state_t *ps_from;
  if (delta < 0) {
    memset(cur, 0, sizeof(*cur)); // uncompressed: from null / baselines
    ps_from = &null_ps;
  } else {
    const sv_demo_snapshot_t *base = &dec->ring[delta & PACKET_MASK];
    if (base->frame_num != delta) {
      Com_Warn("Demo re-encode: delta base frame %d unavailable\n", delta);
      return false;
    }
    ps_from = &base->ps;
    *cur = *base; // unchanged entities persist from the base frame
  }

  Net_ReadDeltaPlayerState(&in, ps_from, &cur->ps);

  for (;;) {
    const int16_t number = Net_ReadShort(&in);
    if (number == -1) {
      break;
    }

    if (number < 0 || number >= MAX_ENTITIES) {
      Com_Warn("Demo re-encode: bad entity number %d\n", number);
      return false;
    }

    const uint16_t bits = Net_ReadShort(&in);

    if (bits & U_REMOVE) {
      cur->active[number] = false;
    } else {
      const entity_state_t *from = cur->active[number]
          ? &cur->entities[number]
          : &dec->baselines[number];
      Net_ReadDeltaEntity(&in, from, &cur->entities[number], number, bits);
      cur->active[number] = true;
    }

    if (in.read > in.size) {
      Com_Warn("Demo re-encode: frame record overran\n");
      return false;
    }
  }

  cur->frame_num = frame_num;
  *out_end = (uint32_t) in.read;
  *out_frame_num = frame_num;
  return true;
}

/**
 * @brief Re-emits the running state as a single SV_CMD_FRAME. A keyframe is a
 * full, self-contained snapshot (player state from null, entities from their
 * baselines) that a seek can land on; a delta frame is encoded against the
 * previous frame, exactly like the live server (mirrors Sv_WriteClientFrame).
 */
static void Sv_DemoEncodeFrame(const sv_demo_snapshot_t *prev, const sv_demo_snapshot_t *cur,
                               const entity_state_t *baselines, mem_buf_t *out,
                               uint32_t frame_num, bool keyframe) {
  static player_state_t null_ps;

  Net_WriteByte(out, SV_CMD_FRAME);
  Net_WriteLong(out, (int32_t) frame_num);
  Net_WriteLong(out, keyframe ? -1 : (int32_t) (frame_num - 1));

  if (keyframe) {
    Net_WriteDeltaPlayerState(out, &null_ps, &cur->ps);
  } else {
    Net_WriteDeltaPlayerState(out, &prev->ps, &cur->ps);
  }

  for (int32_t n = 0; n < MAX_ENTITIES; n++) {
    const bool in_new = cur->active[n];
    const bool in_old = !keyframe && prev->active[n];

    if (in_new && in_old) { // delta from the previous emitted frame's state
      Net_WriteDeltaEntity(out, &prev->entities[n], &cur->entities[n], false);
    } else if (in_new) { // new (or keyframe): delta from the baseline
      Net_WriteDeltaEntity(out, &baselines[n], &cur->entities[n], true);
    } else if (in_old) { // present last frame, gone now
      Net_WriteShort(out, n);
      Net_WriteShort(out, U_REMOVE);
    }
  }

  Net_WriteShort(out, -1); // end of entities
}

/**
 * @brief Streams every record from the recorded demo to the seekable temp file,
 * transforming FRAME records (decode + re-emit as a clean keyframe/delta) and
 * passing reliable, camera, and trailing-datagram bytes through verbatim.
 */
static bool Sv_DemoReencodeRecords(sv_demo_decoder_t *dec, file_t *in, file_t *out) {
  byte in_buf[MAX_MSG_SIZE];
  byte out_buf[MAX_MSG_SIZE * 2];
  demo_record_t rec;
  uint32_t out_frame = 1; // start at 1 so the first delta's base is never 0 (the
                          // client reads delta_frame_num <= 0 as uncompressed)
  int32_t prev_frame_num = -1; // original frame number of the previously emitted frame
  bool have_frame = false;

  for (;;) {
    if (!Demo_ReadRecord(in, &rec, in_buf, sizeof(in_buf))) {
      break; // end of records
    }

    switch (rec.type) {
      case DEMO_RECORD_RELIABLE:
        // the startup epoch arrives as tc-0 reliables before the first frame
        if (rec.timecode == 0 && !have_frame) {
          if (!Sv_DemoParseEpoch(dec, in_buf, rec.length)) {
            return false;
          }
        }
        Demo_WriteRecord(out, DEMO_RECORD_RELIABLE, rec.timecode, in_buf, rec.length);
        break;

      case DEMO_RECORD_FRAME_KEY:
      case DEMO_RECORD_FRAME_DELTA: {
        uint32_t fstart = 0, fend = 0;
        int32_t frame_num = 0;
        if (!Sv_DemoDecodeFrame(dec, in_buf, rec.length, &fstart, &fend, &frame_num)) {
          return false;
        }
        have_frame = true;

        const sv_demo_snapshot_t *cur = &dec->ring[frame_num & PACKET_MASK];
        const sv_demo_snapshot_t *prev =
            (prev_frame_num >= 0) ? &dec->ring[prev_frame_num & PACKET_MASK] : NULL;

        // engine commands the netchan prepended ahead of the frame (older,
        // un-isolated demos): pass them through before the re-encoded frame
        if (fstart > 0) {
          Demo_WriteRecord(out, DEMO_RECORD_RELIABLE, rec.timecode, in_buf, fstart);
        }

        const uint32_t trailing = rec.length - fend;
        mem_buf_t out_msg;

        // emit a keyframe on cadence (and always for the first frame, which has
        // nothing to delta from), but fall back to a smaller delta if the full
        // snapshot plus its trailing datagrams would not fit one message
        bool keyframe = (out_frame % DEMO_KEYFRAME_INTERVAL) == 0 || prev == NULL;
        for (;;) {
          Mem_InitBuffer(&out_msg, out_buf, sizeof(out_buf));
          out_msg.allow_overflow = true; // never abort; detect via .overflowed
          Sv_DemoEncodeFrame(prev, cur, dec->baselines, &out_msg, out_frame, keyframe);

          if (trailing) {
            Mem_WriteBuffer(&out_msg, in_buf + fend, trailing);
          }

          if (!out_msg.overflowed && out_msg.size <= MAX_MSG_SIZE - 16) {
            break; // fits
          }

          if (!keyframe || prev == NULL) {
            // a delta overflowed (the original fit, so this should not happen),
            // or a mandatory keyframe (the first frame) is too large -- give up
            Com_Warn("Demo re-encode: frame %u does not fit (%u bytes)\n",
                     out_frame, (uint32_t) out_msg.size);
            return false;
          }

          keyframe = false; // retry as a delta
        }

        Demo_WriteRecord(out, keyframe ? DEMO_RECORD_FRAME_KEY : DEMO_RECORD_FRAME_DELTA,
                         rec.timecode, out_msg.data, out_msg.size);
        out_frame++;
        prev_frame_num = frame_num;
        break;
      }

      case DEMO_RECORD_CAMERA:
      default:
        Demo_WriteRecord(out, rec.type, rec.timecode, in_buf, rec.length);
        break;
    }
  }

  if (out_frame == 0) {
    Com_Warn("Demo re-encode: no frames decoded\n");
    return false;
  }

  return true;
}

/**
 * @brief Decodes the loaded demo (cursor positioned at its first record) into a
 * clean, fully seekable temp stream and swaps `sv.demo_file` to it. On any
 * failure the original file is left rewound to its first record so forward
 * playback still works.
 * @return True if the seekable stream was built and is now active.
 */
bool Sv_DemoMakeSeekable(const demo_header_t *header) {
  const int64_t first_record = Fs_Tell(sv.demo_file);

  file_t *out = Fs_OpenWrite(SV_DEMO_SEEK_TEMP);
  if (!out) {
    Com_Warn("Couldn't open %s for re-encode\n", SV_DEMO_SEEK_TEMP);
    Fs_Seek(sv.demo_file, first_record);
    return false;
  }

  demo_header_t out_header = *header;
  out_header.epoch_len = 0;

  bool ok = Demo_WriteHeader(out, &out_header, NULL, 0);
  if (ok) {
    sv_demo_decoder_t *dec = Mem_TagMalloc(sizeof(*dec), MEM_TAG_SERVER);
    for (int32_t i = 0; i < PACKET_BACKUP; i++) {
      dec->ring[i].frame_num = -1; // mark every ring slot empty
    }
    ok = Sv_DemoReencodeRecords(dec, sv.demo_file, out);
    Mem_Free(dec);
  }

  Fs_Close(out);

  if (!ok) {
    Fs_Delete(SV_DEMO_SEEK_TEMP);
    Fs_Seek(sv.demo_file, first_record);
    return false;
  }

  file_t *seekable = Fs_OpenRead(SV_DEMO_SEEK_TEMP);
  if (!seekable) {
    Com_Warn("Couldn't reopen re-encoded demo\n");
    Fs_Seek(sv.demo_file, first_record);
    return false;
  }

  demo_header_t tmp;
  if (!Demo_ReadHeader(seekable, &tmp, NULL)) {
    Com_Warn("Re-encoded demo header invalid\n");
    Fs_Close(seekable);
    Fs_Seek(sv.demo_file, first_record);
    return false;
  }

  Fs_Close(sv.demo_file);
  sv.demo_file = seekable;
  return true;
}

/**
 * @brief Removes the seekable scratch file, if present. Called on demo unload.
 */
void Sv_DemoSeekCleanup(void) {
  Fs_Delete(SV_DEMO_SEEK_TEMP);
}
