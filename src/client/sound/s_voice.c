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

#include <opus.h>

#include "s_local.h"

/**
 * @brief Opus operates natively at 48kHz. Voice buffers are queued at that rate rather than
 * resampled to s_rate: OpenAL resamples per buffer with a far better kernel than S_Resample,
 * which is nearest-neighbour and mangles speech.
 */
#define VOICE_RATE 48000

/**
 * @brief The capture and transmission quantum, in milliseconds and in samples.
 */
#define VOICE_FRAME_MILLIS 20
#define VOICE_FRAME_SAMPLES ((VOICE_RATE * VOICE_FRAME_MILLIS) / 1000)

/**
 * @brief Encoded frames held for transmission. The client sends at its frame rate, which may be
 * slower than the 50 frames per second the encoder produces, so a little slack is needed.
 */
#define VOICE_OUT_FRAMES 8

/**
 * @brief Buffers queued on a speaker's source, and how many are banked before playback starts.
 * The queue is the jitter buffer; the pre-roll is what absorbs an irregular packet arrival.
 */
#define VOICE_BUFFERS 8
#define VOICE_PREROLL 2

/**
 * @brief Concurrent speakers rendered. Beyond this the least recently heard is displaced.
 */
#define VOICE_SOURCES 8

/**
 * @brief A speaker falls silent this long after their last frame, whether or not VOICE_END arrived.
 */
#define VOICE_TIMEOUT 400

/**
 * @brief The most consecutive lost frames concealed before a gap is simply accepted.
 */
#define VOICE_MAX_CONCEAL 4

/**
 * @brief The slot used to monitor your own microphone, above the real client slots.
 */
#define VOICE_SELF MAX_CLIENTS

/**
 * @brief The voice thread's tick interval. Bounds how long a completed frame waits before we
 * notice it, so shorter is better; it is a free running thread, not tied to the render loop.
 */
#define VOICE_PUMP_MILLIS 8

/**
 * @brief One speaker being rendered.
 */
typedef struct {
  OpusDecoder *decoder;
  ALuint source;
  ALuint buffers[VOICE_BUFFERS];
  uint32_t time;
  uint8_t seq;
  bool started;
  bool playing;
} s_voice_speaker_t;

static struct {
  bool transmitting;
  bool enabled;

  bool capture_silent;
  int32_t silent_frames;
  float capture_peak;

  OpusEncoder *encoder;

  int16_t frame[VOICE_FRAME_SAMPLES];
  byte payload[VOICE_MAX_PAYLOAD];

  struct {
    byte data[VOICE_MAX_PAYLOAD];
    uint8_t len;
    uint8_t seq;
    uint8_t flags;
  } out[VOICE_OUT_FRAMES];

  int32_t out_head;
  int32_t out_tail;
  // deliberately never reset: a listener measures the gap between sequence numbers to conceal
  // losses, and restarting at zero would read as a jump backwards whenever the final frame of the
  // previous transmission went missing, concealing frames that were never sent
  uint8_t out_seq;
  bool ending;

  uint8_t channel;

  s_voice_speaker_t speakers[MAX_CLIENTS + 1];

  SDL_Thread *thread;
  SDL_Mutex *mutex;
  bool shutdown;
} s_voice_state;

cvar_t *s_voice;
cvar_t *s_voice_bitrate;
cvar_t *s_capture_gain;
cvar_t *s_capture_normalize;
cvar_t *s_voice_loopback;
cvar_t *s_voice_volume;

/**
 * @brief Returns the effective playback gain for voice.
 */
static float S_VoiceGain(void) {
  return Clampf01(s_volume->value) * Clampf01(s_voice_volume->value);
}

/**
 * @brief Warns once if the capture device only ever yields silence.
 * @details macOS denies microphone access by zero filling rather than by failing, and a docked
 * laptop offers a microphone that is simply dead. Both capture perfectly and record nothing, so
 * without this the only symptom is that nobody can hear you.
 */
static void S_CheckCaptureSilence(const int16_t *samples, size_t count) {

  if (s_voice_state.capture_silent) {
    return;
  }

  for (size_t i = 0; i < count; i++) {
    if (samples[i]) {
      s_voice_state.silent_frames = 0;
      return;
    }
  }

  if (++s_voice_state.silent_frames == (1000 / VOICE_FRAME_MILLIS) * 3) {
    Com_Warn("Capture device yielded only silence for 3 seconds.\n"
             "Check that microphone access is granted, and that the device is not muted.\n"
             "Run s_capture_device_list and set s_capture_device to choose another.\n");

    s_voice_state.capture_silent = true;
  }
}

/**
 * @brief Returns the automatic makeup gain for the current speech level.
 * @details A headset at a sensible operating system input level still peaks far below what the
 * game itself plays, so voice at unity disappears under a rocket. The envelope follows peaks
 * immediately and falls away slowly, which tracks how loudly someone is speaking without pumping
 * between syllables, and the gain only ever boosts: a speaker who is already loud is left alone.
 * @remarks Push to talk bounds the damage this can do. An open microphone would have the envelope
 * fall during a silence and amplify the room; a key that has to be held does not.
 */
static float S_CaptureNormalize(const int16_t *samples, size_t count) {

  if (!s_capture_normalize->integer) {
    return 1.f;
  }

  int32_t peak = 0;

  for (size_t i = 0; i < count; i++) {
    const int32_t a = abs(samples[i]);
    if (a > peak) {
      peak = a;
    }
  }

  if (peak > s_voice_state.capture_peak) {
    s_voice_state.capture_peak = peak;
  } else {
    s_voice_state.capture_peak += (peak - s_voice_state.capture_peak) * 0.05f;
  }

  if (s_voice_state.capture_peak < 64.f) {
    return 1.f;
  }

  return Clampf((INT16_MAX * 0.6f) / s_voice_state.capture_peak, 1.f, 16.f);
}

/**
 * @brief Applies automatic and configured microphone gain, clipping rather than wrapping.
 */
static void S_ApplyCaptureGain(int16_t *samples, size_t count) {

  const float gain = S_CaptureNormalize(samples, count) * Clampf(s_capture_gain->value, 0.f, 32.f);

  if (gain == 1.f) {
    return;
  }

  for (size_t i = 0; i < count; i++) {
    samples[i] = (int16_t) Clampf(samples[i] * gain, INT16_MIN, INT16_MAX);
  }
}

/**
 * @brief Releases a speaker's source back to the pool, stopping and unqueueing it.
 */
static void S_ReleaseSpeaker(s_voice_speaker_t *speaker) {

  if (speaker->source) {
    alSourceStop(speaker->source);
    alSourcei(speaker->source, AL_BUFFER, 0);
    alDeleteSources(1, &speaker->source);
    alDeleteBuffers(VOICE_BUFFERS, speaker->buffers);

    speaker->source = 0;
  }

  if (speaker->decoder) {
    opus_decoder_destroy(speaker->decoder);
    speaker->decoder = NULL;
  }

  speaker->started = speaker->playing = false;
}

/**
 * @brief Prepares a speaker to be heard, displacing the least recently heard if the pool is full.
 */
static bool S_AcquireSpeaker(s_voice_speaker_t *speaker, uint8_t flags) {

  if (speaker->source) {
    return true;
  }

  int32_t sources = 0;
  s_voice_speaker_t *oldest = NULL;

  for (size_t i = 0; i < lengthof(s_voice_state.speakers); i++) {
    s_voice_speaker_t *s = s_voice_state.speakers + i;

    if (s->source) {
      sources++;

      if (!oldest || s->time < oldest->time) {
        oldest = s;
      }
    }
  }

  if (sources >= VOICE_SOURCES && oldest) {
    S_ReleaseSpeaker(oldest);
  }

  alGenSources(1, &speaker->source);

  if (!speaker->source) {
    return false;
  }

  alGenBuffers(VOICE_BUFFERS, speaker->buffers);

  alSourcef(speaker->source, AL_GAIN, S_VoiceGain());
  alSourcef(speaker->source, AL_PITCH, 1.f);
  alSourcef(speaker->source, AL_DOPPLER_FACTOR, 0.f);
  alSource3i(speaker->source, AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);

  // Panned toward the speaker for awareness, but never attenuated: a teammate across the map is
  // exactly when a callout matters most. Under AL_LINEAR_DISTANCE_CLAMPED a zero rolloff is unity
  // gain at any distance, while the direction survives.
  alSourcef(speaker->source, AL_ROLLOFF_FACTOR, 0.f);
  alSourcef(speaker->source, AL_REFERENCE_DISTANCE, 256.f);
  alSourcef(speaker->source, AL_MAX_DISTANCE, 2048.f);

  alSourcei(speaker->source, AL_SOURCE_RELATIVE, (flags & VOICE_NO_POS) ? AL_TRUE : AL_FALSE);

  int32_t err;
  speaker->decoder = opus_decoder_create(VOICE_RATE, 1, &err);

  if (err != OPUS_OK) {
    Com_Warn("Couldn't create decoder: %s\n", opus_strerror(err));
    S_ReleaseSpeaker(speaker);
    return false;
  }

  speaker->started = false;
  speaker->playing = false;

  return true;
}

/**
 * @brief Queues one decoded frame on a speaker's source.
 * @details The buffer queue is the jitter buffer. Playback is withheld until VOICE_PREROLL frames
 * are banked, so an irregular arrival does not start and immediately starve, and alSourcePlay is
 * re-issued whenever the source has fallen out of AL_PLAYING, which speech does routinely.
 */
static void S_QueueSpeakerFrame(s_voice_speaker_t *speaker, const int16_t *samples) {

  ALint processed = 0, queued = 0;
  alGetSourcei(speaker->source, AL_BUFFERS_PROCESSED, &processed);
  alGetSourcei(speaker->source, AL_BUFFERS_QUEUED, &queued);

  ALuint buffer;

  if (processed > 0) {
    alSourceUnqueueBuffers(speaker->source, 1, &buffer);
  } else if (queued < VOICE_BUFFERS) {
    buffer = speaker->buffers[queued];
  } else {
    return;
  }

  alBufferData(buffer, AL_FORMAT_MONO16, samples, VOICE_FRAME_SAMPLES * sizeof(int16_t), VOICE_RATE);
  alSourceQueueBuffers(speaker->source, 1, &buffer);

  alSourcef(speaker->source, AL_GAIN, S_VoiceGain());

  alGetSourcei(speaker->source, AL_BUFFERS_QUEUED, &queued);

  if (!speaker->playing && queued < VOICE_PREROLL) {
    return;
  }

  ALint state;
  alGetSourcei(speaker->source, AL_SOURCE_STATE, &state);

  if (state != AL_PLAYING) {
    alSourcePlay(speaker->source);
  }

  speaker->playing = true;
}

/**
 * @brief Decodes one payload for a speaker and queues it, concealing any frames lost before it.
 */
static void S_DecodeSpeakerFrame(s_voice_speaker_t *speaker, const byte *data, int32_t len) {

  int32_t decoded = opus_decode(speaker->decoder, data, len, s_voice_state.frame,
                                VOICE_FRAME_SAMPLES, 0);

  if (decoded != VOICE_FRAME_SAMPLES) {
    Com_Debug(DEBUG_SOUND, "Failed to decode voice: %s\n",
              decoded < 0 ? opus_strerror(decoded) : "short frame");
    return;
  }

  S_QueueSpeakerFrame(speaker, s_voice_state.frame);
}

/**
 * @brief Accepts one voice frame, with s_voice_state.mutex already held.
 * @details Decoding happens on the caller's thread rather than being handed to the voice thread:
 * an Opus frame decodes in tens of microseconds, so even a full server talking at once costs a
 * fraction of a tick, and a second ring would add its own latency for nothing.
 * @remarks The voice thread pumps capture with the lock held, so the local monitor reaches this
 * directly; SDL mutexes are not recursive and the public entry point would deadlock it.
 */
static void S_AddVoice_(int32_t client, uint8_t seq, uint8_t flags, const vec3_t origin,
                        const byte *data, int32_t len) {

  s_voice_speaker_t *speaker = s_voice_state.speakers + client;

  if (S_AcquireSpeaker(speaker, flags)) {

    if (!(flags & VOICE_NO_POS)) {
      alSourcefv(speaker->source, AL_POSITION, origin.xyz);
    }

    if (speaker->started) {
      const uint8_t lost = (uint8_t) (seq - speaker->seq);

      for (uint8_t i = 0; i < lost && i < VOICE_MAX_CONCEAL; i++) {
        if (opus_decode(speaker->decoder, NULL, 0, s_voice_state.frame, VOICE_FRAME_SAMPLES, 0) ==
            VOICE_FRAME_SAMPLES) {
          S_QueueSpeakerFrame(speaker, s_voice_state.frame);
        }
      }
    }

    S_DecodeSpeakerFrame(speaker, data, len);

    speaker->started = true;
    speaker->seq = seq + 1;
    speaker->time = quetoo.ticks;

    if (flags & VOICE_END) {
      speaker->started = false;
    }
  }
}

/**
 * @brief Accepts one voice frame from the network.
 */
void S_AddVoice(int32_t client, uint8_t seq, uint8_t flags, const vec3_t origin,
                const byte *data, int32_t len) {

  if (!s_voice_state.enabled || !s_voice->integer) {
    return;
  }

  if (client < 0 || client > VOICE_SELF) {
    Com_Debug(DEBUG_SOUND, "Rejecting voice from client %d\n", client);
    return;
  }

  if (len <= 0 || len > VOICE_MAX_PAYLOAD) {
    Com_Debug(DEBUG_SOUND, "Rejecting voice payload of %d bytes\n", len);
    return;
  }

  SDL_LockMutex(s_voice_state.mutex);

  S_AddVoice_(client, seq, flags, origin, data, len);

  SDL_UnlockMutex(s_voice_state.mutex);
}

/**
 * @brief Releases speakers that have stopped talking and finished playing.
 */
static void S_ExpireSpeakers(void) {

  for (size_t i = 0; i < lengthof(s_voice_state.speakers); i++) {
    s_voice_speaker_t *speaker = s_voice_state.speakers + i;

    if (!speaker->source) {
      continue;
    }

    if (quetoo.ticks - speaker->time < VOICE_TIMEOUT) {
      continue;
    }

    ALint state;
    alGetSourcei(speaker->source, AL_SOURCE_STATE, &state);

    if (state != AL_PLAYING) {
      S_ReleaseSpeaker(speaker);
    }
  }
}

/**
 * @brief Enqueues one encoded frame for transmission, dropping the oldest if the client is not
 * sending fast enough to keep up.
 */
static void S_EnqueueVoiceFrame(const byte *data, int32_t len, uint8_t flags) {

  if (s_voice_state.out_head - s_voice_state.out_tail == VOICE_OUT_FRAMES) {
    s_voice_state.out_tail++;
  }

  const int32_t i = s_voice_state.out_head++ % VOICE_OUT_FRAMES;

  memcpy(s_voice_state.out[i].data, data, len);
  s_voice_state.out[i].len = (uint8_t) len;
  s_voice_state.out[i].seq = s_voice_state.out_seq++;
  s_voice_state.out[i].flags = flags;
}

/**
 * @brief Hands the next pending voice frame to the caller, returning its length, or 0 if none.
 * @details Called from the client's send path, on the main thread. The packet is written by the
 * caller rather than here, so that the sound library keeps no dependency on the network layer. The
 * channel is carried through opaquely; only the game knows what it means.
 */
int32_t S_ReadVoice(byte *data, uint8_t *seq, uint8_t *flags, uint8_t *channel) {

  if (!s_voice_state.enabled) {
    return 0;
  }

  int32_t len = 0;

  SDL_LockMutex(s_voice_state.mutex);

  if (s_voice_state.out_head != s_voice_state.out_tail) {

    const int32_t i = s_voice_state.out_tail++ % VOICE_OUT_FRAMES;

    len = s_voice_state.out[i].len;

    memcpy(data, s_voice_state.out[i].data, len);
    *seq = s_voice_state.out[i].seq;
    *flags = s_voice_state.out[i].flags;
    *channel = s_voice_state.channel;
  }

  SDL_UnlockMutex(s_voice_state.mutex);

  return len;
}

/**
 * @brief Encodes one captured frame, returning the payload length, or 0 if it could not be encoded.
 */
static int32_t S_EncodeVoiceFrame(const int16_t *samples, byte *payload) {

  const int32_t len = opus_encode(s_voice_state.encoder, samples, VOICE_FRAME_SAMPLES,
                                  payload, VOICE_MAX_PAYLOAD);

  if (len < 0) {
    Com_Warn("Failed to encode voice: %s\n", opus_strerror(len));
    return 0;
  }

  return len;
}

/**
 * @brief Drains the capture device into whole frames, one voice thread tick's worth.
 * @remarks Never opens the device. S_OpenCapture resolves s_capture_device, and cvar strings are
 * freed and replaced by the main thread, so it runs only from S_StartVoice.
 */
static void S_PumpVoice(void) {

  if (!s_voice_state.transmitting || !S_Capturing()) {
    return;
  }

  if (s_voice_bitrate->modified) {
    s_voice_bitrate->modified = false;

    const int32_t bitrate = Clampf(s_voice_bitrate->integer, 6000, 64000);
    opus_encoder_ctl(s_voice_state.encoder, OPUS_SET_BITRATE(bitrate));
  }

  while (S_ReadCapture(s_voice_state.frame, sizeof(s_voice_state.frame)) ==
         (int32_t) sizeof(s_voice_state.frame)) {

    S_CheckCaptureSilence(s_voice_state.frame, VOICE_FRAME_SAMPLES);

    S_ApplyCaptureGain(s_voice_state.frame, VOICE_FRAME_SAMPLES);

    const int32_t len = S_EncodeVoiceFrame(s_voice_state.frame, s_voice_state.payload);

    if (len) {
      S_EnqueueVoiceFrame(s_voice_state.payload, len, 0);

      if (s_voice_loopback->integer) {
        S_AddVoice_(VOICE_SELF, s_voice_state.out_seq - 1, VOICE_NO_POS, Vec3_Zero(),
                    s_voice_state.payload, len);
      }
    }
  }
}

/**
 * @brief Voice thread loop.
 * @details Capture and encoding run on their own clock rather than the render loop's, which varies
 * with framerate and would jitter the 20ms frame cadence.
 */
static int32_t S_VoiceThread(void *data) {

  while (true) {

    SDL_LockMutex(s_voice_state.mutex);

    if (s_voice_state.shutdown) {
      SDL_UnlockMutex(s_voice_state.mutex);
      return 0;
    }

    S_PumpVoice();

    S_ExpireSpeakers();

    SDL_UnlockMutex(s_voice_state.mutex);

    SDL_Delay(VOICE_PUMP_MILLIS);
  }
}

/**
 * @brief Begins a voice transmission on the given channel.
 * @details A device change reopens capture, which also clears a previous failure: latching that
 * permanently would leave a player who picked the wrong microphone with no way back.
 */
void S_StartVoice(uint8_t channel) {

  if (!s_voice_state.enabled || !s_voice->integer) {
    return;
  }

  if (s_capture_device->modified) {
    S_CloseCapture();
  }

  if (!S_OpenCapture(VOICE_RATE)) {
    return;
  }

  SDL_LockMutex(s_voice_state.mutex);

  if (!s_voice_state.transmitting) {
    s_voice_state.transmitting = true;
    s_voice_state.channel = channel;
    s_voice_state.ending = false;

    s_voice_state.capture_silent = false;
    s_voice_state.silent_frames = 0;
    s_voice_state.capture_peak = 0.f;

    S_ResumeCapture();

    opus_encoder_ctl(s_voice_state.encoder, OPUS_RESET_STATE);
  }

  SDL_UnlockMutex(s_voice_state.mutex);
}

/**
 * @brief Ends a voice transmission.
 * @details Pauses the capture device rather than merely ignoring it, so that push to talk does not
 * leave the microphone live, and the operating system's recording indicator goes out with the key.
 * A final empty frame carries VOICE_END, so listeners release the speaker at once rather than
 * waiting out the timeout.
 */
void S_StopVoice(void) {

  if (!s_voice_state.enabled) {
    return;
  }

  SDL_LockMutex(s_voice_state.mutex);

  if (s_voice_state.transmitting) {
    s_voice_state.transmitting = false;

    const int32_t len = S_EncodeVoiceFrame(s_voice_state.frame, s_voice_state.payload);

    if (len) {
      S_EnqueueVoiceFrame(s_voice_state.payload, len, VOICE_END);
    }
  }

  S_PauseCapture();

  SDL_UnlockMutex(s_voice_state.mutex);
}

/**
 * @brief Initializes the voice chat subsystem.
 */
void S_InitVoice(void) {

  memset(&s_voice_state, 0, sizeof(s_voice_state));

  s_voice = Cvar_Add("s_voice", "1", CVAR_ARCHIVE, "Enables voice chat.");
  s_voice_bitrate = Cvar_Add("s_voice_bitrate", "16000", CVAR_ARCHIVE, "Voice chat bitrate, in bits per second.");
  s_capture_gain = Cvar_Add("s_capture_gain", "1", CVAR_ARCHIVE, "Microphone input gain.");
  s_capture_normalize = Cvar_Add("s_capture_normalize", "1", CVAR_ARCHIVE, "Automatically raise a quiet microphone to a usable level.");
  s_voice_loopback = Cvar_Add("s_voice_loopback", "0", CVAR_DEVELOPER, "Play your own microphone back to you (developer tool).");
  s_voice_volume = Cvar_Add("s_voice_volume", "1", CVAR_ARCHIVE, "Voice chat volume.");

  s_voice_state.mutex = SDL_CreateMutex();

  if (!s_voice_state.mutex) {
    Com_Warn("Couldn't create mutex: %s\n", SDL_GetError());
    return;
  }

  int32_t err;

  s_voice_state.encoder = opus_encoder_create(VOICE_RATE, 1, OPUS_APPLICATION_VOIP, &err);

  if (err != OPUS_OK) {
    Com_Warn("Couldn't create encoder: %s\n", opus_strerror(err));
    S_ShutdownVoice();
    return;
  }

  opus_encoder_ctl(s_voice_state.encoder, OPUS_SET_BITRATE(Clampf(s_voice_bitrate->integer, 6000, 64000)));
  opus_encoder_ctl(s_voice_state.encoder, OPUS_SET_COMPLEXITY(5));
  opus_encoder_ctl(s_voice_state.encoder, OPUS_SET_INBAND_FEC(1));
  opus_encoder_ctl(s_voice_state.encoder, OPUS_SET_PACKET_LOSS_PERC(10));
  opus_encoder_ctl(s_voice_state.encoder, OPUS_SET_DTX(0));

  s_voice_state.thread = SDL_CreateThread(S_VoiceThread, __func__, NULL);

  if (!s_voice_state.thread) {
    Com_Warn("Couldn't create thread: %s\n", SDL_GetError());
    S_ShutdownVoice();
    return;
  }

  s_voice_state.enabled = true;

  Com_Print("Voice initialized (%s)\n", opus_get_version_string());
}

/**
 * @brief Releases every speaker, so that a level change does not leave the dead talking.
 */
void S_StopVoices(void) {

  if (!s_voice_state.enabled) {
    return;
  }

  SDL_LockMutex(s_voice_state.mutex);

  for (size_t i = 0; i < lengthof(s_voice_state.speakers); i++) {
    S_ReleaseSpeaker(s_voice_state.speakers + i);
  }

  s_voice_state.out_head = s_voice_state.out_tail = 0;

  SDL_UnlockMutex(s_voice_state.mutex);
}

/**
 * @brief Shuts down the voice chat subsystem, releasing all of its resources.
 */
void S_ShutdownVoice(void) {

  s_voice_state.enabled = false;

  if (s_voice_state.thread) {
    SDL_LockMutex(s_voice_state.mutex);
    s_voice_state.shutdown = true;
    SDL_UnlockMutex(s_voice_state.mutex);

    SDL_WaitThread(s_voice_state.thread, NULL);
  }

  S_CloseCapture();

  for (size_t i = 0; i < lengthof(s_voice_state.speakers); i++) {
    S_ReleaseSpeaker(s_voice_state.speakers + i);
  }

  if (s_voice_state.encoder) {
    opus_encoder_destroy(s_voice_state.encoder);
  }

  if (s_voice_state.mutex) {
    SDL_DestroyMutex(s_voice_state.mutex);
  }

  memset(&s_voice_state, 0, sizeof(s_voice_state));
}
