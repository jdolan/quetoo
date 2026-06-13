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

#include "cl_local.h"
#include "common/demo.h"

cvar_t *cl_demo_v2;

// state for the in-progress recording
static bool cl_demo_recording_v2; // true if the active recording is the v2 container
static int32_t cl_demo_first_frame; // frame_num of the first recorded frame, or -1
static int32_t cl_demo_last_frame; // frame_num of the most recently recorded frame
static int32_t cl_demo_last_keyframe; // frame_num of the most recently recorded keyframe

/**
 * @brief Flushes one accumulated startup sub-message to the demo file, as a v1
 * length-prefixed block or, for v2, a RELIABLE record stamped at timecode 0.
 */
static void Cl_FlushDemoHeader(mem_buf_t *msg) {

  if (msg->size == 0) {
    return;
  }

  if (cl_demo_recording_v2) {
    Demo_WriteRecord(cls.demo_file, DEMO_RECORD_RELIABLE, 0, msg->data, msg->size);
  } else {
    const int32_t len = LittleLong((int32_t) msg->size);
    Fs_Write(cls.demo_file, &len, sizeof(len), 1);
    Fs_Write(cls.demo_file, msg->data, msg->size, 1);
  }

  msg->size = 0;
}

/**
 * @brief Writes `server_data`, `config_strings`, and baselines once a non-delta
 * compressed frame arrives from the server. For v2 demos this also writes the
 * container header; the startup messages become RELIABLE records at timecode 0.
 */
static void Cl_WriteDemoHeader(void) {
  static entity_state_t null_state;
  mem_buf_t msg;
  byte buffer[MAX_MSG_SIZE];

  if (cl_demo_recording_v2) {
    const demo_header_t header = {
      .version = DEMO_FORMAT_VERSION,
      .flags = DEMO_FLAG_CLIENT,
      .protocol_major = PROTOCOL_MAJOR,
      .protocol_minor = (uint16_t) cls.cgame->protocol,
      .tick_rate = QUETOO_TICK_RATE,
      .epoch_len = 0,
    };
    Demo_WriteHeader(cls.demo_file, &header, NULL, 0);
  }

  // write out messages to hold the startup information
  Mem_InitBuffer(&msg, buffer, sizeof(buffer));

  // write the server data
  Net_WriteByte(&msg, SV_CMD_SERVER_DATA);
  Net_WriteLong(&msg, PROTOCOL_MAJOR);
  Net_WriteLong(&msg, cls.cgame->protocol);
  Net_WriteByte(&msg, 1); // demo_server byte
  Net_WriteString(&msg, Cvar_GetString("game"));
  Net_WriteString(&msg, cl.config_strings[CS_MESSAGE]);

  // and config_strings
  for (int32_t i = 0; i < MAX_CONFIG_STRINGS; i++) {
    if (*cl.config_strings[i] != '\0') {
      if (msg.size + strlen(cl.config_strings[i]) + 32 > msg.max_size) { // write it out
        Cl_FlushDemoHeader(&msg);
      }

      Net_WriteByte(&msg, SV_CMD_CONFIG_STRING);
      Net_WriteShort(&msg, i);
      Net_WriteString(&msg, cl.config_strings[i]);
    }
  }

  // and baselines
  for (size_t i = 0; i < lengthof(cl.entities); i++) {
    entity_state_t *ent = &cl.entities[i].baseline;
    if (i != 0 && !ent->number) { // entity 0 is worldspawn; never skip it
      continue;
    }

    if (msg.size + 64 > msg.max_size) { // write it out
      Cl_FlushDemoHeader(&msg);
    }

    Net_WriteByte(&msg, SV_CMD_BASELINE);
    Net_WriteDeltaEntity(&msg, &null_state, &cl.entities[i].baseline, true);
  }

  Net_WriteByte(&msg, SV_CMD_CBUF_TEXT);
  Net_WriteString(&msg, "precache 0\n");

  Cl_FlushDemoHeader(&msg);

  Com_Debug(DEBUG_CLIENT, "Demo started\n");
  // the rest of the demo file will be individual frames
}

/**
 * @brief Re-encodes the current decoded frame as a self-contained keyframe (a
 * full, non-delta snapshot) and writes it as a FRAME_KEY record. This is the
 * exact wire format the client parser expects for an uncompressed frame: the
 * player state delta'd from null, and every live entity delta'd from its
 * baseline. Seeking jumps to these keyframes (see Demo_ScanIndex / Sv_DemoSeek).
 */
static void Cl_WriteDemoKeyframe(uint32_t timecode) {
  static player_state_t null_state;
  mem_buf_t kf;
  byte buffer[MAX_MSG_SIZE];

  Mem_InitBuffer(&kf, buffer, sizeof(buffer));

  Net_WriteByte(&kf, SV_CMD_FRAME);
  Net_WriteLong(&kf, cl.frame.frame_num);
  Net_WriteLong(&kf, -1); // uncompressed

  Net_WriteDeltaPlayerState(&kf, &null_state, &cl.frame.ps);

  for (int32_t i = 0; i < cl.frame.num_entities; i++) {
    const uint32_t snum = (cl.frame.entity_state + i) & ENTITY_STATE_MASK;
    const entity_state_t *s = &cl.entity_states[snum];
    Net_WriteDeltaEntity(&kf, &cl.entities[s->number].baseline, s, true);
  }

  Net_WriteShort(&kf, -1); // end of entities

  Demo_WriteRecord(cls.demo_file, DEMO_RECORD_FRAME_KEY, timecode, kf.data, kf.size);
}

/**
 * @brief Delta-encodes the entities of `to` against `from`, mirroring the
 * server's Sv_WriteEntities. Both entity lists are sorted by number.
 */
static void Cl_WriteDemoEntitiesDelta(const cl_frame_t *from, const cl_frame_t *to, mem_buf_t *msg) {

  int32_t old_index = 0, new_index = 0;

  while (new_index < to->num_entities || old_index < from->num_entities) {

    const entity_state_t *new_state = (new_index < to->num_entities)
        ? &cl.entity_states[(to->entity_state + new_index) & ENTITY_STATE_MASK] : NULL;
    const entity_state_t *old_state = (old_index < from->num_entities)
        ? &cl.entity_states[(from->entity_state + old_index) & ENTITY_STATE_MASK] : NULL;

    const int16_t new_num = new_state ? new_state->number : INT16_MAX;
    const int16_t old_num = old_state ? old_state->number : INT16_MAX;

    if (new_num == old_num) { // delta update
      Net_WriteDeltaEntity(msg, old_state, new_state, false);
      old_index++;
      new_index++;
    } else if (new_num < old_num) { // new entity from baseline
      Net_WriteDeltaEntity(msg, &cl.entities[new_num].baseline, new_state, true);
      new_index++;
    } else { // removed entity
      Net_WriteShort(msg, old_num);
      Net_WriteShort(msg, U_REMOVE);
      old_index++;
    }
  }

  Net_WriteShort(msg, -1); // end of entities
}

/**
 * @brief Re-encodes the current frame as a delta from the immediately preceding
 * recorded frame. Used when the original packet's delta references a frame
 * older than the last keyframe, which would break playback after a seek. Falls
 * back to a keyframe if the previous frame isn't available.
 */
static void Cl_WriteDemoFrameDelta(uint32_t timecode) {

  const cl_frame_t *prev = &cl.frames[(cl.frame.frame_num - 1) & PACKET_MASK];

  if (!prev->valid || prev->frame_num != cl.frame.frame_num - 1) {
    Cl_WriteDemoKeyframe(timecode);
    cl_demo_last_keyframe = cl.frame.frame_num;
    return;
  }

  mem_buf_t msg;
  byte buffer[MAX_MSG_SIZE];
  Mem_InitBuffer(&msg, buffer, sizeof(buffer));

  Net_WriteByte(&msg, SV_CMD_FRAME);
  Net_WriteLong(&msg, cl.frame.frame_num);
  Net_WriteLong(&msg, cl.frame.frame_num - 1); // delta from the previous frame

  Net_WriteDeltaPlayerState(&msg, &prev->ps, &cl.frame.ps);
  Cl_WriteDemoEntitiesDelta(prev, &cl.frame, &msg);

  Demo_WriteRecord(cls.demo_file, DEMO_RECORD_FRAME_DELTA, timecode, msg.data, msg.size);
}

/**
 * @brief Writes the current net message as a v2 record, stamping it with the
 * current frame's timecode and classifying it as a keyframe, delta, or reliable.
 * Every DEMO_KEYFRAME_INTERVAL frames the natural delta is replaced with a
 * re-encoded keyframe so that seeking has a nearby self-contained entry point.
 * Any delta that references a frame older than the last keyframe is rebased so
 * that playback after a seek can always be reconstructed from the keyframe.
 */
static void Cl_WriteDemoRecord(void) {

  if (cl_demo_first_frame < 0) {
    cl_demo_first_frame = cl.frame.frame_num;
  }

  const int32_t rel_frame = cl.frame.frame_num - cl_demo_first_frame;
  const uint32_t timecode = (uint32_t) rel_frame * QUETOO_TICK_MILLIS;

  if (cl.frame.frame_num == cl_demo_last_frame) { // a trailing datagram-only packet
    Demo_WriteRecord(cls.demo_file, DEMO_RECORD_RELIABLE, timecode, net_message.data + 8, net_message.size - 8);
    return;
  }

  cl_demo_last_frame = cl.frame.frame_num;

  if (cl.frame.delta_frame_num < 0) { // a naturally uncompressed frame is already a keyframe
    Demo_WriteRecord(cls.demo_file, DEMO_RECORD_FRAME_KEY, timecode, net_message.data + 8, net_message.size - 8);
    cl_demo_last_keyframe = cl.frame.frame_num;
  } else if (rel_frame > 0 && (rel_frame % DEMO_KEYFRAME_INTERVAL) == 0) { // periodic keyframe
    Cl_WriteDemoKeyframe(timecode);
    cl_demo_last_keyframe = cl.frame.frame_num;
  } else if (cl.frame.delta_frame_num < cl_demo_last_keyframe) { // delta reaches before the keyframe
    Cl_WriteDemoFrameDelta(timecode);
  } else {
    Demo_WriteRecord(cls.demo_file, DEMO_RECORD_FRAME_DELTA, timecode, net_message.data + 8, net_message.size - 8);
  }
}

/**
 * @brief Dumps the current net message to the demo, prefixed by the length (v1)
 * or wrapped in a timecoded record (v2).
 */
void Cl_WriteDemoMessage(void) {

  if (!cls.demo_file) {
    return;
  }

  if (!Fs_Tell(cls.demo_file)) {
    if (cl.frame.delta_frame_num < 0) {
      Com_Debug(DEBUG_CLIENT, "Received uncompressed frame, writing demo header..\n");
      Cl_WriteDemoHeader();
    } else {
      return; // wait for an uncompressed packet
    }
  }

  if (cl_demo_recording_v2) {
    Cl_WriteDemoRecord();
  } else {
    // the first eight bytes are just packet sequencing stuff
    const int32_t len = LittleLong((int32_t) (net_message.size - 8));

    Fs_Write(cls.demo_file, &len, sizeof(len), 1);
    Fs_Write(cls.demo_file, net_message.data + 8, len, 1);
  }
}

/**
 * @brief Stop recording a demo
 */
void Cl_Stop_f(void) {

  if (!cls.demo_file) {
    Com_Print("Not recording a demo\n");
    return;
  }

  if (!cl_demo_recording_v2) { // v1 demos are terminated with a -1 length sentinel
    int32_t len = -1;
    Fs_Write(cls.demo_file, &len, sizeof(len), 1);
  }

  // v2 demos are terminated by end-of-file (records are self-framing)
  Fs_Close(cls.demo_file);

  cls.demo_file = NULL;
  Com_Print("Stopped demo\n");
}

/**
 * @brief record <demo name>
 *
 * Begin recording a demo from the current frame until `stop` is issued.
 */
void Cl_Record_f(void) {

  if (Cmd_Argc() > 2) {
    Com_Print("Usage: %s [demo name]\n", Cmd_Argv(0));
    return;
  }

  if (cls.demo_file) {
    Com_Print("Already recording\n");
    return;
  }

  if (cls.state != CL_ACTIVE) {
    Com_Print("You must be in a level to record\n");
    return;
  }

  if (Cmd_Argc() == 2) {
    g_snprintf(cls.demo_filename, sizeof(cls.demo_filename), "demos/%s.demo", Cmd_Argv(1));
  } else {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char datestamp[32];
    strftime(datestamp, sizeof(datestamp), "%Y-%m-%d-%H-%M-%S", tm);

    g_snprintf(cls.demo_filename, sizeof(cls.demo_filename), "demos/%s.demo", datestamp);
  }

  // open the demo file
  if (!(cls.demo_file = Fs_OpenWrite(cls.demo_filename))) {
    Com_Warn("Couldn't open %s\n", cls.demo_filename);
    return;
  }

  cl_demo_recording_v2 = cl_demo_v2->value;
  cl_demo_first_frame = -1;
  cl_demo_last_frame = -1;
  cl_demo_last_keyframe = -1;

  Com_Print("Recording to %s (%s)\n", cls.demo_filename, cl_demo_recording_v2 ? "v2" : "v1");
}

#define DEMO_PLAYBACK_STEP 1

/**
 * @brief Adjusts time scale by delta, clamping to reasonable limits.
 */
static void Cl_AdjustDemoPlayback(float delta) {

  if (!cl.demo_server) {
    return;
  }

  Cvar_ForceSetValue(time_scale->name, Clampf(time_scale->value + delta, DEMO_PLAYBACK_STEP, 4.0));

  Com_Print("Demo playback rate %d%%\n", (int32_t) (time_scale->value * 100));
}

/**
 * @brief Handles the `cl_fast_forward` command, increasing demo playback speed.
 */
void Cl_FastForward_f(void) {
  Cl_AdjustDemoPlayback(DEMO_PLAYBACK_STEP);
}

/**
 * @brief Handles the `cl_slow_motion` command, decreasing demo playback speed.
 */
void Cl_SlowMotion_f(void) {
  Cl_AdjustDemoPlayback(-DEMO_PLAYBACK_STEP);
}

/**
 * @brief Registers demo recording cvars.
 */
void Cl_InitDemo(void) {
  cl_demo_v2 = Cvar_Add("cl_demo_v2", "1", CVAR_ARCHIVE,
      "Record demos in the seekable v2 container format (0 for legacy v1).");
}
