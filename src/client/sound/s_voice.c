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
} SoundVoiceSpeaker;

static struct {
  bool transmitting;
  bool enabled;

  bool captureSilent;
  int32_t silentFrames;
  float capturePeak;

  OpusEncoder *encoder;

  int16_t frame[VOICE_FRAME_SAMPLES];
  byte payload[VOICE_MAX_PAYLOAD];

  struct {
    byte data[VOICE_MAX_PAYLOAD];
    uint8_t len;
    uint8_t seq;
    uint8_t flags;
    uint8_t channel;
  } out[VOICE_OUT_FRAMES];

  int32_t outHead;
  int32_t outTail;
  // deliberately never reset: a listener measures the gap between sequence numbers to conceal
  // losses, and restarting at zero would read as a jump backwards whenever the final frame of the
  // previous transmission went missing, concealing frames that were never sent
  uint8_t outSeq;
  bool ending;

  uint8_t channel;

  SoundVoiceSpeaker speakers[MAX_CLIENTS + 1];

  SDL_Thread *thread;
  SDL_Mutex *mutex;
  bool shutdown;
} module;

Cvar *s_voice;
Cvar *s_voiceBitrate;
Cvar *s_captureGain;
Cvar *s_captureNormalize;
Cvar *s_voiceLoopback;
Cvar *s_voiceVolume;

/**
 * @brief Returns the effective playback gain for voice.
 */
static float S_VoiceGain(void) {
  return Clampf01(s_volume->value) * Clampf01(s_voiceVolume->value);
}

/**
 * @brief Warns once if the capture device only ever yields silence.
 * @details macOS denies microphone access by zero filling rather than by failing, and a docked
 * laptop offers a microphone that is simply dead. Both capture perfectly and record nothing, so
 * without this the only symptom is that nobody can hear you.
 */
static void S_CheckCaptureSilence(const int16_t *samples, size_t count) {

  if (module.captureSilent) {
    return;
  }

  for (size_t i = 0; i < count; i++) {
    if (samples[i]) {
      module.silentFrames = 0;
      return;
    }
  }

  if (++module.silentFrames == (1000 / VOICE_FRAME_MILLIS) * 3) {
    Com_Warn("Capture device yielded only silence for 3 seconds.\n"
             "Check that microphone access is granted, and that the device is not muted.\n"
             "Run s_capture_device_list and set s_capture_device to choose another.\n");

    module.captureSilent = true;
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

  if (!s_captureNormalize->integer) {
    return 1.f;
  }

  int32_t peak = 0;

  for (size_t i = 0; i < count; i++) {
    const int32_t a = abs(samples[i]);
    if (a > peak) {
      peak = a;
    }
  }

  if (peak > module.capturePeak) {
    module.capturePeak = peak;
  } else {
    module.capturePeak += (peak - module.capturePeak) * 0.05f;
  }

  if (module.capturePeak < 64.f) {
    return 1.f;
  }

  return Clampf((INT16_MAX * 0.6f) / module.capturePeak, 1.f, 16.f);
}

/**
 * @brief Applies automatic and configured microphone gain, clipping rather than wrapping.
 */
static void S_ApplyCaptureGain(int16_t *samples, size_t count) {

  const float gain = S_CaptureNormalize(samples, count) * Clampf(s_captureGain->value, 0.f, 32.f);

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
static void S_ReleaseSpeaker(SoundVoiceSpeaker *speaker) {

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
static bool S_AcquireSpeaker(SoundVoiceSpeaker *speaker) {

  if (speaker->source) {
    return true;
  }

  int32_t sources = 0;
  SoundVoiceSpeaker *oldest = NULL;

  for (size_t i = 0; i < lengthof(module.speakers); i++) {
    SoundVoiceSpeaker *s = module.speakers + i;

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

  // Voice is never spatialized: a relative source at the listener's own origin keeps a callout
  // equally audible whoever makes it, and whatever the listener happens to be facing.
  alSourcei(speaker->source, AL_SOURCE_RELATIVE, AL_TRUE);

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
static void S_QueueSpeakerFrame(SoundVoiceSpeaker *speaker, const int16_t *samples) {

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
static void S_DecodeSpeakerFrame(SoundVoiceSpeaker *speaker, const byte *data, int32_t len) {

  int32_t decoded = opus_decode(speaker->decoder, data, len, module.frame,
                                VOICE_FRAME_SAMPLES, 0);

  if (decoded != VOICE_FRAME_SAMPLES) {
    Com_Debug(DEBUG_SOUND, "Failed to decode voice: %s\n",
              decoded < 0 ? opus_strerror(decoded) : "short frame");
    return;
  }

  S_QueueSpeakerFrame(speaker, module.frame);
}

/**
 * @brief Accepts one voice frame, with module.mutex already held.
 * @details Decoding happens on the caller's thread rather than being handed to the voice thread:
 * an Opus frame decodes in tens of microseconds, so even a full server talking at once costs a
 * fraction of a tick, and a second ring would add its own latency for nothing.
 * @remarks The voice thread pumps capture with the lock held, so the local monitor reaches this
 * directly; SDL mutexes are not recursive and the public entry point would deadlock it.
 */
static void S_AddVoice_(int32_t client, uint8_t seq, uint8_t flags, const byte *data, int32_t len) {

  SoundVoiceSpeaker *speaker = module.speakers + client;

  if (S_AcquireSpeaker(speaker)) {

    if (speaker->started) {
      const uint8_t lost = (uint8_t) (seq - speaker->seq);

      for (uint8_t i = 0; i < lost && i < VOICE_MAX_CONCEAL; i++) {
        if (opus_decode(speaker->decoder, NULL, 0, module.frame, VOICE_FRAME_SAMPLES, 0) ==
            VOICE_FRAME_SAMPLES) {
          S_QueueSpeakerFrame(speaker, module.frame);
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
void S_AddVoice(int32_t client, uint8_t seq, uint8_t flags, const byte *data, int32_t len) {

  if (!module.enabled || !s_voice->integer) {
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

  SDL_LockMutex(module.mutex);

  S_AddVoice_(client, seq, flags, data, len);

  SDL_UnlockMutex(module.mutex);
}

/**
 * @brief Releases speakers that have stopped talking and finished playing.
 */
static void S_ExpireSpeakers(void) {

  for (size_t i = 0; i < lengthof(module.speakers); i++) {
    SoundVoiceSpeaker *speaker = module.speakers + i;

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
 * @remarks The frame remembers its own channel. A transmission's last frames, its VOICE_END above
 * all, can still be queued when the next key down picks a different channel, and they belong to the
 * transmission that produced them.
 */
static void S_EnqueueVoiceFrame(const byte *data, int32_t len, uint8_t flags) {

  if (module.outHead - module.outTail == VOICE_OUT_FRAMES) {
    module.outTail++;
  }

  const int32_t i = module.outHead++ % VOICE_OUT_FRAMES;

  memcpy(module.out[i].data, data, len);
  module.out[i].len = (uint8_t) len;
  module.out[i].seq = module.outSeq++;
  module.out[i].flags = flags;
  module.out[i].channel = module.channel;
}

/**
 * @brief Hands the next pending voice frame to the caller, returning its length, or 0 if none.
 * @details Called from the client's send path, on the main thread. The packet is written by the
 * caller rather than here, so that the sound library keeps no dependency on the network layer. The
 * channel is carried through opaquely; only the game knows what it means.
 */
int32_t S_ReadVoice(byte *data, uint8_t *seq, uint8_t *flags, uint8_t *channel) {

  if (!module.enabled) {
    return 0;
  }

  int32_t len = 0;

  SDL_LockMutex(module.mutex);

  if (module.outHead != module.outTail) {

    const int32_t i = module.outTail++ % VOICE_OUT_FRAMES;

    len = module.out[i].len;

    memcpy(data, module.out[i].data, len);
    *seq = module.out[i].seq;
    *flags = module.out[i].flags;
    *channel = module.out[i].channel;
  }

  SDL_UnlockMutex(module.mutex);

  return len;
}

/**
 * @brief Encodes one captured frame, returning the payload length, or 0 if it could not be encoded.
 */
static int32_t S_EncodeVoiceFrame(const int16_t *samples, byte *payload) {

  const int32_t len = opus_encode(module.encoder, samples, VOICE_FRAME_SAMPLES,
                                  payload, VOICE_MAX_PAYLOAD);

  if (len < 0) {
    Com_Warn("Failed to encode voice: %s\n", opus_strerror(len));
    return 0;
  }

  return len;
}

/**
 * @brief Drains the capture device into whole frames, one voice thread tick's worth.
 * @remarks Never opens the device. S_OpenCapture resolves s_captureDevice, and cvar strings are
 * freed and replaced by the main thread, so it runs only from S_StartVoice.
 */
static void S_PumpVoice(void) {

  if (!module.transmitting || !S_Capturing()) {
    return;
  }

  if (s_voiceBitrate->modified) {
    s_voiceBitrate->modified = false;

    const int32_t bitrate = Clampf(s_voiceBitrate->integer, 6000, 64000);
    opus_encoder_ctl(module.encoder, OPUS_SET_BITRATE(bitrate));
  }

  while (S_ReadCapture(module.frame, sizeof(module.frame)) ==
         (int32_t) sizeof(module.frame)) {

    S_CheckCaptureSilence(module.frame, VOICE_FRAME_SAMPLES);

    S_ApplyCaptureGain(module.frame, VOICE_FRAME_SAMPLES);

    const int32_t len = S_EncodeVoiceFrame(module.frame, module.payload);

    if (len) {
      S_EnqueueVoiceFrame(module.payload, len, 0);

      if (s_voiceLoopback->integer) {
        S_AddVoice_(VOICE_SELF, module.outSeq - 1, 0, module.payload, len);
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

    SDL_LockMutex(module.mutex);

    if (module.shutdown) {
      SDL_UnlockMutex(module.mutex);
      return 0;
    }

    S_PumpVoice();

    S_ExpireSpeakers();

    SDL_UnlockMutex(module.mutex);

    SDL_Delay(VOICE_PUMP_MILLIS);
  }
}

/**
 * @brief Begins a voice transmission on the given channel.
 * @details A device change reopens capture, which also clears a previous failure: latching that
 * permanently would leave a player who picked the wrong microphone with no way back.
 */
void S_StartVoice(uint8_t channel) {

  if (!module.enabled || !s_voice->integer) {
    return;
  }

  if (s_captureDevice->modified) {
    S_CloseCapture();
  }

  if (!S_OpenCapture(VOICE_RATE)) {
    return;
  }

  SDL_LockMutex(module.mutex);

  if (!module.transmitting) {
    module.transmitting = true;
    module.channel = channel;
    module.ending = false;

    module.captureSilent = false;
    module.silentFrames = 0;
    module.capturePeak = 0.f;

    S_ResumeCapture();

    opus_encoder_ctl(module.encoder, OPUS_RESET_STATE);
  }

  SDL_UnlockMutex(module.mutex);
}

/**
 * @brief Ends a voice transmission.
 * @details Pauses the capture device rather than merely ignoring it, so that push to talk does not
 * leave the microphone live, and the operating system's recording indicator goes out with the key.
 * A final empty frame carries VOICE_END, so listeners release the speaker at once rather than
 * waiting out the timeout.
 */
void S_StopVoice(void) {

  if (!module.enabled) {
    return;
  }

  SDL_LockMutex(module.mutex);

  if (module.transmitting) {
    module.transmitting = false;

    const int32_t len = S_EncodeVoiceFrame(module.frame, module.payload);

    if (len) {
      S_EnqueueVoiceFrame(module.payload, len, VOICE_END);
    }
  }

  S_PauseCapture();

  SDL_UnlockMutex(module.mutex);
}

/**
 * @brief Initializes the voice chat subsystem.
 */
void S_InitVoice(void) {

  memset(&module, 0, sizeof(module));

  s_voice = Cvar_Add("s_voice", "1", CVAR_ARCHIVE, "Enables voice chat.");
  s_voiceBitrate = Cvar_Add("s_voice_bitrate", "16000", CVAR_ARCHIVE, "Voice chat bitrate, in bits per second.");
  s_captureGain = Cvar_Add("s_capture_gain", "1", CVAR_ARCHIVE, "Microphone input gain.");
  s_captureNormalize = Cvar_Add("s_capture_normalize", "1", CVAR_ARCHIVE, "Automatically raise a quiet microphone to a usable level.");
  s_voiceLoopback = Cvar_Add("s_voice_loopback", "0", CVAR_DEVELOPER, "Play your own microphone back to you (developer tool).");
  s_voiceVolume = Cvar_Add("s_voice_volume", "1", CVAR_ARCHIVE, "Voice chat volume.");

  module.mutex = SDL_CreateMutex();

  if (!module.mutex) {
    Com_Warn("Couldn't create mutex: %s\n", SDL_GetError());
    return;
  }

  int32_t err;

  module.encoder = opus_encoder_create(VOICE_RATE, 1, OPUS_APPLICATION_VOIP, &err);

  if (err != OPUS_OK) {
    Com_Warn("Couldn't create encoder: %s\n", opus_strerror(err));
    S_ShutdownVoice();
    return;
  }

  opus_encoder_ctl(module.encoder, OPUS_SET_BITRATE(Clampf(s_voiceBitrate->integer, 6000, 64000)));
  opus_encoder_ctl(module.encoder, OPUS_SET_COMPLEXITY(5));
  opus_encoder_ctl(module.encoder, OPUS_SET_INBAND_FEC(1));
  opus_encoder_ctl(module.encoder, OPUS_SET_PACKET_LOSS_PERC(10));
  opus_encoder_ctl(module.encoder, OPUS_SET_DTX(0));

  module.thread = SDL_CreateThread(S_VoiceThread, __func__, NULL);

  if (!module.thread) {
    Com_Warn("Couldn't create thread: %s\n", SDL_GetError());
    S_ShutdownVoice();
    return;
  }

  module.enabled = true;

  Com_Print("Voice initialized (%s)\n", opus_get_version_string());
}

/**
 * @brief Releases every speaker, so that a level change does not leave the dead talking.
 */
void S_StopVoices(void) {

  if (!module.enabled) {
    return;
  }

  SDL_LockMutex(module.mutex);

  for (size_t i = 0; i < lengthof(module.speakers); i++) {
    S_ReleaseSpeaker(module.speakers + i);
  }

  module.outHead = module.outTail = 0;

  SDL_UnlockMutex(module.mutex);
}

/**
 * @brief Shuts down the voice chat subsystem, releasing all of its resources.
 */
void S_ShutdownVoice(void) {

  module.enabled = false;

  if (module.thread) {
    SDL_LockMutex(module.mutex);
    module.shutdown = true;
    SDL_UnlockMutex(module.mutex);

    SDL_WaitThread(module.thread, NULL);
  }

  S_CloseCapture();

  for (size_t i = 0; i < lengthof(module.speakers); i++) {
    S_ReleaseSpeaker(module.speakers + i);
  }

  if (module.encoder) {
    opus_encoder_destroy(module.encoder);
  }

  if (module.mutex) {
    SDL_DestroyMutex(module.mutex);
  }

  memset(&module, 0, sizeof(module));
}
