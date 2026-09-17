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

/**
 * @brief A generous per-entity margin (comfortably more than Net_WriteDeltaEntity's worst case,
 * e.g. every field changed) reserved before each entity write in Cl_WriteDemoMessage, so a busy
 * frame stops recording entities before it could overflow the buffer, rather than letting
 * Mem_WriteBuffer's fatal error take down the whole client over one dropped demo frame.
 */
#define DEMO_ENTITY_MARGIN 128

/**
 * @brief Writes a length + frame_num prefixed chunk to the demo file.
 */
static void Cl_WriteDemoChunk(const void *data, size_t size, int32_t frame_num) {
  const int32_t len = LittleLong((int32_t) size);
  const int32_t num = LittleLong(frame_num);

  Fs_Write(cls.demo.file, &len, sizeof(len), 1);
  Fs_Write(cls.demo.file, &num, sizeof(num), 1);
  Fs_Write(cls.demo.file, data, size, 1);
}

/**
 * @brief Appends a frame's location to the in-memory index accumulated while recording.
 */
static void Cl_AddDemoKeyframe(int32_t frame_num, int32_t offset) {

  if (cls.demo.num_keyframes == cls.demo.max_keyframes) {
    cls.demo.max_keyframes = cls.demo.max_keyframes ? cls.demo.max_keyframes * 2 : 64;
    cls.demo.keyframes = Mem_Realloc(cls.demo.keyframes,
                                      cls.demo.max_keyframes * sizeof(demo_keyframe_t));
  }

  cls.demo.keyframes[cls.demo.num_keyframes].frame_num = frame_num;
  cls.demo.keyframes[cls.demo.num_keyframes].offset = offset;
  cls.demo.num_keyframes++;
}

/**
 * @brief Writes the fixed-size header, `server_data`, `config_strings`, and baselines. Called
 * once, lazily, from the first `Cl_WriteDemoMessage` call after `record`: baselines are already
 * fully populated by then, since reaching `CL_ACTIVE` (a precondition for `record`) requires
 * having received them at connect time.
 */
static void Cl_WriteDemoHeader(void) {
  static entity_state_t null_state;
  mem_buf_t msg;
  byte buffer[MAX_MSG_SIZE];

  demo_header_t *header = &cls.demo.header;
  memset(header, 0, sizeof(*header));

  memcpy(header->magic, DEMO_MAGIC, sizeof(header->magic));
  header->version = LittleLong(DEMO_VERSION);
  q_strlcpy(header->map, cl.config_strings[CS_BSP], sizeof(header->map));
  q_strlcpy(header->message, cl.config_strings[CS_MESSAGE], sizeof(header->message));
  header->title[0] = '\0';
  header->favorite = 0;
  header->duration = 0;
  header->num_keyframes = 0;
  header->ofs_keyframes = 0;

  Fs_Write(cls.demo.file, header, sizeof(*header), 1);

  // write out messages to hold the startup information
  Mem_InitBuffer(&msg, buffer, sizeof(buffer));

  // write the server data
  Net_WriteByte(&msg, SV_CMD_SERVER_DATA);
  Net_WriteLong(&msg, PROTOCOL_MAJOR);
  Net_WriteLong(&msg, cls.cgame->protocol);
  Net_WriteByte(&msg, 1); // demo_server byte
  Net_WriteString(&msg, Com_Game());
  Net_WriteString(&msg, Com_Cgame());
  Net_WriteString(&msg, cl.config_strings[CS_MESSAGE]);

  // and config_strings
  for (int32_t i = 0; i < MAX_CONFIG_STRINGS; i++) {
    if (*cl.config_strings[i] != '\0') {
      if (msg.size + q_strlen(cl.config_strings[i]) + 32 > msg.max_size) { // write it out
        Cl_WriteDemoChunk(msg.data, msg.size, 0);
        msg.size = 0;
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
      Cl_WriteDemoChunk(msg.data, msg.size, 0);
      msg.size = 0;
    }

    Net_WriteByte(&msg, SV_CMD_BASELINE);
    Net_WriteDeltaEntity(&msg, &null_state, &cl.entities[i].baseline, true);
  }

  Net_WriteByte(&msg, SV_CMD_CBUF_TEXT);
  Net_WriteString(&msg, "precache 0\n");

  // write it to the demo file
  Cl_WriteDemoChunk(msg.data, msg.size, 0);

  Com_Debug(DEBUG_CLIENT, "Demo started\n");
  // the rest of the demo file will be individual frames
}

/**
 * @brief Writes the current frame to the demo file as a fully self-contained record: every live
 * entity delta-encoded against its baseline, and the player state delta-encoded against a null
 * state - the exact wire shape the server itself uses for a genuine uncompressed frame
 * (`Sv_WriteClientFrame`/`Sv_WriteEntities` with `from == NULL`). This is written regardless of
 * whether the frame actually received over the wire was itself delta-compressed: the client
 * already holds the fully-resolved state in `cl.entities`/`cl.frame` either way, so every
 * recorded frame is independently decodable by the ordinary parse path, with no dependency on
 * any other frame or on periodic server resyncs. Any one-shot commands captured this packet by
 * `Cl_ParseServerMessage` (chat, centerprint, temp entities, sounds) ride along verbatim.
 */
void Cl_WriteDemoMessage(void) {

  if (!cls.demo.file) {
    return;
  }

  if (Fs_Tell(cls.demo.file) == 0) {
    Cl_WriteDemoHeader();
    cls.demo.last_frame_num = -1;
  }

  if (!cl.frame.valid || cl.frame.frame_num == cls.demo.last_frame_num) {
    return; // this packet carried no new frame to record
  }

  if (cls.demo.start_frame_num < 0) {
    cls.demo.start_frame_num = cl.frame.frame_num;
  }

  // every frame_num persisted to the file is relative to start_frame_num, so the file's own
  // numbering always starts at 0 - duration and Sv_SeekDemo's millis-to-frame_num conversion
  // both assume this
  const int32_t frame_num = cl.frame.frame_num - cls.demo.start_frame_num;

  cls.demo.last_frame_num = cl.frame.frame_num;

  Cl_AddDemoKeyframe(frame_num, (int32_t) Fs_Tell(cls.demo.file));

  static player_state_t null_ps;

  // bounded by MAX_MSG_SIZE to match what Sv_GetDemoMessage accepts as a valid chunk on
  // playback, and what the server's own relay buffers and Netchan_Transmit can actually carry in
  // one message - unlike a plain on-disk record size, this isn't a purely local concern.
  mem_buf_t msg;
  byte buffer[MAX_MSG_SIZE];
  Mem_InitBuffer(&msg, buffer, sizeof(buffer));

  Net_WriteByte(&msg, SV_CMD_FRAME);
  Net_WriteLong(&msg, frame_num);

  // -1: every recorded frame is an "uncompressed" frame, matching the server's own convention, so
  // entities always decode from baseline rather than chaining to another recorded frame. This no
  // longer disables interpolation: cl.previous_frame (sequential frame_num continuity) drives
  // that independently of cl.delta_frame now, so sequential playback still blends smoothly, while
  // a real discontinuity (a seek) is still correctly detected and snapped.
  Net_WriteLong(&msg, -1);

  Net_WriteDeltaPlayerState(&msg, &null_ps, &cl.frame.ps);

  int32_t entities_dropped = 0;
  for (int32_t i = 0; i < cl.frame.num_entities; i++) {
    if (msg.max_size - msg.size < DEMO_ENTITY_MARGIN) {
      entities_dropped = cl.frame.num_entities - i;
      break;
    }
    const uint32_t snum = (cl.frame.entity_state + i) & ENTITY_STATE_MASK;
    const entity_state_t *s = &cl.entity_states[snum];
    Net_WriteDeltaEntity(&msg, &cl.entities[s->number].baseline, s, true);
  }
  Net_WriteShort(&msg, -1);

  if (entities_dropped) {
    Com_Warn("Demo frame %d too large: dropped %d of %d entities\n",
              frame_num, entities_dropped, cl.frame.num_entities);
  }

  if (cls.demo.event_size) {
    if (msg.size + cls.demo.event_size <= msg.max_size) {
      Mem_WriteBuffer(&msg, cls.demo.event_buffer, cls.demo.event_size);
    } else {
      Com_Warn("Demo frame %d too large: dropped %zu bytes of events\n", frame_num, cls.demo.event_size);
    }
    cls.demo.event_size = 0;
  }

  Cl_WriteDemoChunk(msg.data, msg.size, frame_num);
}

/**
 * @brief Stop recording a demo
 */
void Cl_Stop_f(void) {
  int32_t len = -1;

  if (!cls.demo.file) {
    Com_Print("Not recording a demo\n");
    return;
  }

  // terminate the frame stream
  Fs_Write(cls.demo.file, &len, sizeof(len), 1);

  if (!memcmp(cls.demo.header.magic, DEMO_MAGIC, sizeof(cls.demo.header.magic))) { // a header was actually written

    const int32_t ofs_keyframes = (int32_t) Fs_Tell(cls.demo.file);

    for (size_t i = 0; i < cls.demo.num_keyframes; i++) {
      demo_keyframe_t entry = cls.demo.keyframes[i];
      entry.frame_num = LittleLong(entry.frame_num);
      entry.offset = LittleLong(entry.offset);
      Fs_Write(cls.demo.file, &entry, sizeof(entry), 1);
    }

    // duration is relative to start_frame_num, matching every frame_num persisted to the file
    // (see Cl_WriteDemoMessage) - cl.frame.frame_num alone is the absolute server tick count
    // since map load, not since recording started
    const int32_t frames_recorded = cls.demo.start_frame_num < 0 ? 0 :
        cl.frame.frame_num - cls.demo.start_frame_num;

    // patch the in-memory copy of the header rather than reading it back: cls.demo.file is
    // opened write-only, so Fs_Read on it would silently fail and leave the header stack garbage
    cls.demo.header.duration = LittleLong((int32_t) (frames_recorded * QUETOO_TICK_MILLIS));
    cls.demo.header.num_keyframes = LittleLong((int32_t) cls.demo.num_keyframes);
    cls.demo.header.ofs_keyframes = LittleLong(ofs_keyframes);

    Fs_Seek(cls.demo.file, 0);
    Fs_Write(cls.demo.file, &cls.demo.header, sizeof(cls.demo.header), 1);
  }

  Fs_Close(cls.demo.file);

  cls.demo.file = NULL;
  Mem_Free(cls.demo.keyframes);
  cls.demo.keyframes = NULL;
  cls.demo.num_keyframes = cls.demo.max_keyframes = 0;
  cls.demo.start_frame_num = -1;

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

  if (cls.demo.file) {
    Com_Print("Already recording\n");
    return;
  }

  if (cls.state != CL_ACTIVE) {
    Com_Print("You must be in a level to record\n");
    return;
  }

  if (Cmd_Argc() == 2) {
    q_snprintf(cls.demo.filename, sizeof(cls.demo.filename), "demos/%s.demo", Cmd_Argv(1));
  } else {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char datestamp[32];
    strftime(datestamp, sizeof(datestamp), "%Y-%m-%d-%H-%M-%S", tm);

    q_snprintf(cls.demo.filename, sizeof(cls.demo.filename), "demos/%s.demo", datestamp);
  }

  // open the demo file
  if (!(cls.demo.file = Fs_OpenWrite(cls.demo.filename))) {
    Com_Warn("Couldn't open %s\n", cls.demo.filename);
    return;
  }

  Mem_Free(cls.demo.keyframes);
  cls.demo.keyframes = NULL;
  cls.demo.num_keyframes = cls.demo.max_keyframes = 0;
  cls.demo.last_frame_num = -1;
  cls.demo.start_frame_num = -1;
  cls.demo.event_size = 0;
  memset(&cls.demo.header, 0, sizeof(cls.demo.header));

  Com_Print("Recording to %s\n", cls.demo.filename);
}

/**
 * @brief The demo playback rates, ascending.
 * @remarks This table MUST be kept in step with the `values` of the speed slider in
 * `ui/hud/DemoControlsView.json`, and MUST stay within the `time_scale` bounds enforced
 * by `main.c`.
 */
static const float demo_playback_speeds[] = { 0.25f, 0.5f, 0.75f, 1.f, 2.f, 3.f };

/**
 * @brief
 */
static void Cl_SetDemoPlaybackSpeed(ssize_t index) {

  if (!cl.demo_server) {
    return;
  }

  index = SDL_clamp(index, 0, (ssize_t) lengthof(demo_playback_speeds) - 1);

  Cvar_ForceSetValue(time_scale->name, demo_playback_speeds[index]);

  Com_Print("Demo playback rate %d%%\n", (int32_t) (time_scale->value * 100));
}

/**
 * @return The index in `demo_playback_speeds` nearest the current `time_scale`.
 */
static size_t Cl_DemoPlaybackSpeedIndex(void) {

  size_t index = 0;
  float nearest = FLT_MAX;

  for (size_t i = 0; i < lengthof(demo_playback_speeds); i++) {
    const float delta = fabsf(demo_playback_speeds[i] - time_scale->value);
    if (delta < nearest) {
      nearest = delta;
      index = i;
    }
  }

  return index;
}

/**
 * @brief
 */
void Cl_SetDemoPlaybackSpeed_f(void) {

  const char *arg = Cmd_Argv(1);

  if (Cmd_Argc() != 2 || !SDL_isdigit(*arg)) {
    Com_Print("Usage: %s [0-%d]\n", Cmd_Argv(0), (int32_t) lengthof(demo_playback_speeds) - 1);
    return;
  }

  Cl_SetDemoPlaybackSpeed((ssize_t) strtol(arg, NULL, 10));
}

/**
 * @brief
 */
static void Cl_SetDemoPlaybackSpeedRelative(int32_t increment) {

  if (!cl.demo_server) {
    return;
  }

  Cl_SetDemoPlaybackSpeed((ssize_t) Cl_DemoPlaybackSpeedIndex() + increment);
}

/**
 * @brief Handles the `demo_playback_faster` command, increasing demo playback speed.
 */
void Cl_DemoPlaybackFaster_f(void) {
  Cl_SetDemoPlaybackSpeedRelative(+1);
}

/**
 * @brief Handles the `demo_playback_slower` command, decreasing demo playback speed.
 */
void Cl_DemoPlaybackSlower_f(void) {
  Cl_SetDemoPlaybackSpeedRelative(-1);
}

/**
 * @brief Handles the `demo_pause` command by forwarding it to the demo relay, which owns pause
 * state and reports it back via @c SV_CMD_DEMO_INFO. Nothing is toggled locally: the server pauses
 * on its own when playback reaches the last frame, and a local guess would desync from that.
 * `cls.demo.paused` then frees the mouse and shows the cursor so the transport controls can be
 * clicked, all without leaving @c KEY_GAME - @c KEY_UI would hide the HUD, and those controls with it.
 */
void Cl_DemoPause_f(void) {

  if (!cl.demo_server) {
    return;
  }

  Cl_ForwardCmdToServer();
}
