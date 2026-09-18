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
 * @brief Buffers queued on a voice source, and the capture device's ring size.
 */
#define VOICE_BUFFERS 8
#define VOICE_CAPTURE_SAMPLES (VOICE_RATE / 2)

/**
 * @brief The voice thread's tick interval. Deliberately shorter than QUETOO_TICK_MILLIS, which
 * would alias badly against the 20ms frame quantum.
 */
#define VOICE_PUMP_MILLIS 10

typedef struct {
  SDL_AudioStream *capture;
  bool capture_failed;
  bool capture_silent;
  int32_t silent_frames;
  bool transmitting;

  ALuint source;
  ALuint buffers[VOICE_BUFFERS];

  int16_t frame[VOICE_FRAME_SAMPLES];

  SDL_Thread *thread;
  SDL_Mutex *mutex;
  bool shutdown;
} s_voice_state_t;

static s_voice_state_t s_voice_state;

cvar_t *s_voice;
cvar_t *s_voice_device;
cvar_t *s_voice_gain;
cvar_t *s_voice_loopback;
cvar_t *s_voice_volume;

/**
 * @brief Returns the effective playback gain for voice.
 */
static float S_VoiceGain(void) {
  return Clampf01(s_volume->value) * Clampf01(s_voice_volume->value);
}

/**
 * @brief Prints the available capture devices, for use with s_voice_device.
 */
static void S_VoiceDevices_f(void) {

  int32_t count = 0;
  SDL_AudioDeviceID *devices = SDL_GetAudioRecordingDevices(&count);

  if (!devices || !count) {
    Com_Print("No capture devices available\n");
    SDL_free(devices);
    return;
  }

  Com_Print("Capture devices:\n");

  for (int32_t i = 0; i < count; i++) {
    Com_Print("  ^2%s^7\n", SDL_GetAudioDeviceName(devices[i]));
  }

  SDL_free(devices);
}

/**
 * @brief Resolves s_voice_device to a recording device, falling back to the system default.
 */
static SDL_AudioDeviceID S_CaptureDevice(void) {

  if (!s_voice_device->string[0]) {
    return SDL_AUDIO_DEVICE_DEFAULT_RECORDING;
  }

  int32_t count = 0;
  SDL_AudioDeviceID *devices = SDL_GetAudioRecordingDevices(&count);
  SDL_AudioDeviceID device = SDL_AUDIO_DEVICE_DEFAULT_RECORDING;

  for (int32_t i = 0; i < count; i++) {
    const char *name = SDL_GetAudioDeviceName(devices[i]);
    if (name && !q_strcmp(name, s_voice_device->string)) {
      device = devices[i];
      break;
    }
  }

  if (device == SDL_AUDIO_DEVICE_DEFAULT_RECORDING) {
    Com_Warn("Capture device \"%s\" not found, using the default\n", s_voice_device->string);
  }

  SDL_free(devices);
  return device;
}

/**
 * @brief Opens and starts the capture device, warning once if it is unavailable.
 * @details Capture is opened on first transmission rather than at initialization, so that players
 * who never speak are never prompted for microphone access and never light the recording indicator.
 */
static bool S_OpenCapture(void) {

  if (s_voice_state.capture) {
    return true;
  }

  if (s_voice_state.capture_failed) {
    return false;
  }

  const SDL_AudioDeviceID device = S_CaptureDevice();

  const SDL_AudioSpec spec = {
    .format = SDL_AUDIO_S16,
    .channels = 1,
    .freq = VOICE_RATE,
  };

  s_voice_state.capture = SDL_OpenAudioDeviceStream(device, &spec, NULL, NULL);

  if (!s_voice_state.capture) {
    Com_Warn("Failed to open capture device: %s\n", SDL_GetError());
    s_voice_state.capture_failed = true;
    return false;
  }

  Com_Print("Voice capture opened (%s)\n", SDL_GetAudioDeviceName(SDL_GetAudioStreamDevice(s_voice_state.capture)));
  return true;
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
             "Run s_voice_devices and set s_voice_device to choose another.\n");
    s_voice_state.capture_silent = true;
  }
}

/**
 * @brief Applies the configured microphone gain, clipping rather than wrapping.
 */
static void S_ApplyCaptureGain(int16_t *samples, size_t count) {

  const float gain = Clampf(s_voice_gain->value, 0.f, 4.f);

  if (gain == 1.f) {
    return;
  }

  for (size_t i = 0; i < count; i++) {
    samples[i] = (int16_t) Clampf(samples[i] * gain, INT16_MIN, INT16_MAX);
  }
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
 */
static void S_PumpVoice(void) {

  if (!s_voice_state.transmitting) {
    return;
  }

  if (!S_OpenCapture()) {
    return;
  }

  while (SDL_GetAudioStreamAvailable(s_voice_state.capture) >= (int32_t) sizeof(s_voice_state.frame)) {

    if (SDL_GetAudioStreamData(s_voice_state.capture, s_voice_state.frame,
                               sizeof(s_voice_state.frame)) != (int32_t) sizeof(s_voice_state.frame)) {
      break;
    }

    S_CheckCaptureSilence(s_voice_state.frame, VOICE_FRAME_SAMPLES);

    S_ApplyCaptureGain(s_voice_state.frame, VOICE_FRAME_SAMPLES);

    if (s_voice_loopback->integer) {
      S_QueueVoiceFrame(s_voice_state.frame);
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

  if (!s_voice || !s_voice->integer) {
    return;
  }

  SDL_LockMutex(s_voice_state.mutex);

  if (s_voice_device->modified) {
    s_voice_device->modified = false;

    if (s_voice_state.capture) {
      SDL_DestroyAudioStream(s_voice_state.capture);
      s_voice_state.capture = NULL;
    }

    s_voice_state.capture_failed = false;
    s_voice_state.capture_silent = false;
    s_voice_state.silent_frames = 0;
  }

  if (!s_voice_state.transmitting && S_OpenCapture()) {
    s_voice_state.transmitting = true;

    SDL_ClearAudioStream(s_voice_state.capture);
    SDL_ResumeAudioStreamDevice(s_voice_state.capture);
  }

  SDL_UnlockMutex(s_voice_state.mutex);
}

/**
 * @brief Ends a voice transmission.
 * @details Pauses the capture device rather than merely ignoring it, so that push to talk does not
 * leave the microphone live, and the operating system's recording indicator goes out with the key.
 */
void S_StopVoice(void) {

  if (!s_voice) {
    return;
  }

  SDL_LockMutex(s_voice_state.mutex);

  s_voice_state.transmitting = false;

  if (s_voice_state.capture) {
    SDL_PauseAudioStreamDevice(s_voice_state.capture);
    SDL_ClearAudioStream(s_voice_state.capture);
  }

  SDL_UnlockMutex(s_voice_state.mutex);
}

/**
 * @brief Initializes the voice chat subsystem.
 */
void S_InitVoice(void) {

  memset(&s_voice_state, 0, sizeof(s_voice_state));

  s_voice = Cvar_Add("s_voice", "1", CVAR_ARCHIVE, "Enables voice chat.");
  s_voice_device = Cvar_Add("s_voice_device", "", CVAR_ARCHIVE, "The microphone to capture from, or empty for the system default.");
  s_voice_gain = Cvar_Add("s_voice_gain", "1", CVAR_ARCHIVE, "Microphone input gain.");
  s_voice_loopback = Cvar_Add("s_voice_loopback", "0", CVAR_DEVELOPER, "Play your own microphone back to you (developer tool).");
  s_voice_volume = Cvar_Add("s_voice_volume", "1", CVAR_ARCHIVE, "Voice chat volume.");

  Cmd_Add("s_voice_devices", S_VoiceDevices_f, CMD_SOUND, "List the available microphones.");

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

  s_voice_state.mutex = SDL_CreateMutex();

  if (!s_voice_state.mutex) {
    Com_Warn("Couldn't create mutex: %s\n", SDL_GetError());
    S_ShutdownVoice();
    return;
  }

  s_voice_state.thread = SDL_CreateThread(S_VoiceThread, __func__, NULL);

  if (!s_voice_state.thread) {
    Com_Warn("Couldn't create thread: %s\n", SDL_GetError());
    S_ShutdownVoice();
    return;
  }

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

  if (s_voice_state.capture) {
    SDL_DestroyAudioStream(s_voice_state.capture);
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
