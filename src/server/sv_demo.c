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
