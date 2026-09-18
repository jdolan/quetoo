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
 * @brief The largest Opus payload accepted for one frame. Generous for 20ms of speech at any
 * sane bitrate, and small enough that a malformed length is rejected before the decoder sees it.
 */
#define VOICE_MAX_PAYLOAD 128

/**
 * @brief Buffers queued on a voice source, and the capture device's ring size.
 */
#define VOICE_BUFFERS 8
#define VOICE_CAPTURE_SAMPLES (VOICE_RATE / 2)

/**
 * @brief The voice thread's tick interval. Bounds how long a completed frame waits before we
 * notice it, so shorter is better; it is a free running thread, not tied to the render loop.
 */
#define VOICE_PUMP_MILLIS 8

static struct {
  bool capture_silent;
  int32_t silent_frames;
  bool transmitting;

  ALuint source;
  ALuint buffers[VOICE_BUFFERS];

  OpusEncoder *encoder;
  OpusDecoder *decoder;

  int16_t frame[VOICE_FRAME_SAMPLES];
  byte payload[VOICE_MAX_PAYLOAD];

  SDL_Thread *thread;
  SDL_Mutex *mutex;
  bool shutdown;
  bool enabled;
} s_voice_state;

cvar_t *s_voice;
cvar_t *s_voice_bitrate;
cvar_t *s_capture_gain;
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
 * @details macOS denies microphone access by zero filling rather than by failing, so a build the
 * system has not granted access to captures perfectly and records nothing. Without this the only
 * symptom is that nobody can hear you.
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
 * @brief Applies the configured microphone gain, clipping rather than wrapping.
 */
static void S_ApplyCaptureGain(int16_t *samples, size_t count) {

  const float gain = Clampf(s_capture_gain->value, 0.f, 4.f);

  if (gain == 1.f) {
    return;
  }

  for (size_t i = 0; i < count; i++) {
    samples[i] = (int16_t) Clampf(samples[i] * gain, INT16_MIN, INT16_MAX);
  }
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
 * @brief Decodes one voice payload into a frame of mono PCM.
 * @details The length is validated before the decoder sees it, and the output buffer is fixed at
 * one frame. These bytes will arrive from the network, so the decoder is never handed a length it
 * did not ask for, nor asked to write more than a frame: a packet encoding more than 20ms fails
 * here rather than overrunning.
 */
static bool S_DecodeVoiceFrame(const byte *payload, int32_t len, int16_t *samples) {

  if (len <= 0 || len > VOICE_MAX_PAYLOAD) {
    Com_Debug(DEBUG_SOUND, "Rejecting voice payload of %d bytes\n", len);
    return false;
  }

  const int32_t decoded = opus_decode(s_voice_state.decoder, payload, len, samples, VOICE_FRAME_SAMPLES, 0);

  if (decoded != VOICE_FRAME_SAMPLES) {
    Com_Debug(DEBUG_SOUND, "Failed to decode voice: %s\n",
              decoded < 0 ? opus_strerror(decoded) : "short frame");
    return false;
  }

  return true;
}

/**
 * @brief Queues one frame of mono PCM on the voice source, recycling processed buffers.
 * @details Unlike S_BufferMusic, this re-issues alSourcePlay whenever the source has fallen out of
 * AL_PLAYING. A streaming source that starves stays stopped otherwise, and speech starves routinely.
 */
static void S_QueueVoiceFrame(const int16_t *samples) {

  ALint processed = 0, queued = 0;
  alGetSourcei(s_voice_state.source, AL_BUFFERS_PROCESSED, &processed);
  alGetSourcei(s_voice_state.source, AL_BUFFERS_QUEUED, &queued);

  ALuint buffer;

  if (processed > 0) {
    alSourceUnqueueBuffers(s_voice_state.source, 1, &buffer);
  } else if (queued < VOICE_BUFFERS) {
    buffer = s_voice_state.buffers[queued];
  } else {
    return;
  }

  alBufferData(buffer, AL_FORMAT_MONO16, samples, VOICE_FRAME_SAMPLES * sizeof(int16_t), VOICE_RATE);
  alSourceQueueBuffers(s_voice_state.source, 1, &buffer);

  alSourcef(s_voice_state.source, AL_GAIN, S_VoiceGain());

  ALint state;
  alGetSourcei(s_voice_state.source, AL_SOURCE_STATE, &state);

  if (state != AL_PLAYING) {
    alSourcePlay(s_voice_state.source);
  }
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

  while (S_ReadCapture(s_voice_state.frame, sizeof(s_voice_state.frame)) == (int32_t) sizeof(s_voice_state.frame)) {

    S_CheckCaptureSilence(s_voice_state.frame, VOICE_FRAME_SAMPLES);

    S_ApplyCaptureGain(s_voice_state.frame, VOICE_FRAME_SAMPLES);

    const int32_t len = S_EncodeVoiceFrame(s_voice_state.frame, s_voice_state.payload);

    if (s_voice_loopback->integer && len) {
      if (S_DecodeVoiceFrame(s_voice_state.payload, len, s_voice_state.frame)) {
        S_QueueVoiceFrame(s_voice_state.frame);
      }
    }
  }
}

/**
 * @brief Voice thread loop.
 * @details Capture and playback run on their own clock rather than the render loop's, which varies
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

    SDL_UnlockMutex(s_voice_state.mutex);

    SDL_Delay(VOICE_PUMP_MILLIS);
  }
}

/**
 * @brief Begins a voice transmission, opening the capture device if this is the first one.
 * @details A device change reopens capture, which also clears a previous failure: latching that
 * permanently would leave a player who picked the wrong microphone with no way back.
 */
void S_StartVoice(void) {

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

    s_voice_state.capture_silent = false;
    s_voice_state.silent_frames = 0;

    S_ResumeCapture();

    opus_encoder_ctl(s_voice_state.encoder, OPUS_RESET_STATE);
    opus_decoder_ctl(s_voice_state.decoder, OPUS_RESET_STATE);
  }

  SDL_UnlockMutex(s_voice_state.mutex);
}

/**
 * @brief Ends a voice transmission.
 * @details Pauses the capture device rather than merely ignoring it, so that push to talk does not
 * leave the microphone live, and the operating system's recording indicator goes out with the key.
 */
void S_StopVoice(void) {

  if (!s_voice_state.enabled) {
    return;
  }

  SDL_LockMutex(s_voice_state.mutex);

  s_voice_state.transmitting = false;

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
  s_voice_loopback = Cvar_Add("s_voice_loopback", "0", CVAR_DEVELOPER, "Play your own microphone back to you (developer tool).");
  s_voice_volume = Cvar_Add("s_voice_volume", "1", CVAR_ARCHIVE, "Voice chat volume.");

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

  s_voice_state.decoder = opus_decoder_create(VOICE_RATE, 1, &err);

  if (err != OPUS_OK) {
    Com_Warn("Couldn't create decoder: %s\n", opus_strerror(err));
    S_ShutdownVoice();
    return;
  }

  s_voice_state.mutex = SDL_CreateMutex();

  if (!s_voice_state.mutex) {
    Com_Warn("Couldn't create mutex: %s\n", SDL_GetError());
    return;
  }

  alGenSources(1, &s_voice_state.source);

  if (!s_voice_state.source) {
    Com_Warn("Couldn't allocate source: %s\n", alGetString(alGetError()));
    return;
  }

  alGenBuffers(VOICE_BUFFERS, s_voice_state.buffers);

  if (!*s_voice_state.buffers) {
    Com_Warn("Couldn't allocate buffers: %s\n", alGetString(alGetError()));
    return;
  }

  alSourcef(s_voice_state.source, AL_GAIN, S_VoiceGain());
  alSourcei(s_voice_state.source, AL_SOURCE_RELATIVE, AL_TRUE);
  alSourcef(s_voice_state.source, AL_ROLLOFF_FACTOR, 0.f);
  alSourcef(s_voice_state.source, AL_DOPPLER_FACTOR, 0.f);
  alSourcef(s_voice_state.source, AL_PITCH, 1.f);
  alSource3i(s_voice_state.source, AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);

  S_GetError(NULL);

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
 * @brief Shuts down the voice chat subsystem, releasing all of its resources.
 */
void S_ShutdownVoice(void) {

  if (s_voice_state.thread) {
    SDL_LockMutex(s_voice_state.mutex);
    s_voice_state.shutdown = true;
    SDL_UnlockMutex(s_voice_state.mutex);

    SDL_WaitThread(s_voice_state.thread, NULL);
  }

  S_CloseCapture();

  if (s_voice_state.encoder) {
    opus_encoder_destroy(s_voice_state.encoder);
  }

  if (s_voice_state.decoder) {
    opus_decoder_destroy(s_voice_state.decoder);
  }

  if (s_voice_state.source) {
    alDeleteSources(1, &s_voice_state.source);
    alDeleteBuffers(VOICE_BUFFERS, s_voice_state.buffers);

    S_GetError(NULL);
  }

  if (s_voice_state.mutex) {
    SDL_DestroyMutex(s_voice_state.mutex);
  }

  memset(&s_voice_state, 0, sizeof(s_voice_state));
}
