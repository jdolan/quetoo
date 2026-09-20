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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "s_local.h"

/**
 * @brief The stereo frame size of the loopback device's render format.
 */
#define S_FRAME_SIZE (sizeof(int16_t) * 2)

static struct {
  SDL_AudioStream *playback;
  SDL_AudioStream *capture;
  bool captureFailed;
  bool warnedSharedDevice;
} module;

Cvar *s_bufferFrames;
Cvar *s_captureDevice;

/**
 * @brief Prints the devices in the given list.
 */
static void S_DeviceList(SDL_AudioDeviceID *devices, int32_t count, const char *what) {

  if (!devices || !count) {
    Com_Print("No %s devices available\n", what);
    SDL_free(devices);
    return;
  }

  Com_Print("%s devices:\n", what);

  for (int32_t i = 0; i < count; i++) {
    Com_Print("  ^2%s^7\n", SDL_GetAudioDeviceName(devices[i]));
  }

  SDL_free(devices);
}

/**
 * @brief Prints the available microphones, for use with s_captureDevice.
 */
static void S_CaptureDeviceList_f(void) {
  int32_t count = 0;
  S_DeviceList(SDL_GetAudioRecordingDevices(&count), count, "Capture");
}

/**
 * @brief Prints the available speakers.
 */
static void S_PlaybackDeviceList_f(void) {
  int32_t count = 0;
  S_DeviceList(SDL_GetAudioPlaybackDevices(&count), count, "Playback");
}

/**
 * @brief Renders mixed audio on demand for the playback device.
 * @details The loopback device runs no thread of its own: nothing is mixed until this asks for it.
 * Letting SDL pull means SDL's device keeps the clock, and there is no feeder cadence to tune or
 * to drift.
 */
static void S_RenderSamples(void *data, SDL_AudioStream *stream, int32_t additional, int32_t total) {

  static byte buffer[16384];

  while (additional >= (int32_t) S_FRAME_SIZE) {

    const int32_t bytes = additional < (int32_t) sizeof(buffer) ? additional : (int32_t) sizeof(buffer);
    const int32_t samples = bytes / S_FRAME_SIZE;

    alcRenderSamplesSOFT(sContext.device, buffer, samples);
    SDL_PutAudioStreamData(stream, buffer, samples * (int32_t) S_FRAME_SIZE);

    additional -= samples * (int32_t) S_FRAME_SIZE;
  }
}

/**
 * @brief Opens the playback device that drives the loopback renderer.
 */
bool S_InitPlayback(void) {

  if (s_bufferFrames->integer > 0) {
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, va("%d", s_bufferFrames->integer));
  } else {
    SDL_ResetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES);
  }

  const SDL_AudioSpec spec = {
    .format = SDL_AUDIO_S16,
    .channels = 2,
    .freq = s_rate->integer,
  };

  module.playback = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, S_RenderSamples, NULL);

  if (!module.playback) {
    Com_Warn("Failed to open playback device: %s\n", SDL_GetError());
    return false;
  }

  SDL_ResumeAudioStreamDevice(module.playback);

  SDL_AudioSpec deviceSpec;
  int32_t deviceFrames = 0;

  if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(module.playback), &deviceSpec, &deviceFrames)) {
    Com_Print("  Playback:   ^2%s^7\n", SDL_GetAudioDeviceName(SDL_GetAudioStreamDevice(module.playback)));
    Com_Print("  Buffer:     ^2%d frames (%.1fms) @ %dhz^7\n", deviceFrames,
              deviceFrames * 1000.f / deviceSpec.freq, deviceSpec.freq);
  }

  return true;
}

/**
 * @brief Closes the playback device.
 * @remarks Called before OpenAL teardown, so that an in-flight render callback cannot reach a
 * context that is being destroyed.
 */
void S_ShutdownPlayback(void) {

  if (module.playback) {
    SDL_DestroyAudioStream(module.playback);
    module.playback = NULL;
  }
}

/**
 * @brief Warns when the microphone and the speakers are the same device.
 * @details A Bluetooth headset cannot carry a microphone and high quality audio at once. Opening
 * its microphone moves it from A2DP to the hands free profile, and the operating system drops
 * everything the game plays to 16kHz mono for as long as the key is held. Nothing can be done
 * about that from here, but a player deserves to know why their audio changed.
 */
static void S_CheckSharedDevice(const char *capture) {

  if (!module.playback || module.warnedSharedDevice) {
    return;
  }

  const char *playback = SDL_GetAudioDeviceName(SDL_GetAudioStreamDevice(module.playback));

  if (playback && capture && !q_strcmp(playback, capture)) {
    Com_Warn("Microphone and speakers are both \"%s\".\n"
             "If this is a Bluetooth headset, audio quality will drop while you transmit.\n"
             "Run s_captureDeviceList and set s_captureDevice to a separate microphone to avoid it.\n",
             capture);

    module.warnedSharedDevice = true;
  }
}

/**
 * @brief Resolves s_captureDevice to a recording device, falling back to the system default.
 */
static SDL_AudioDeviceID S_CaptureDevice(void) {

  if (!s_captureDevice->string[0]) {
    return SDL_AUDIO_DEVICE_DEFAULT_RECORDING;
  }

  int32_t count = 0;
  SDL_AudioDeviceID *devices = SDL_GetAudioRecordingDevices(&count);
  SDL_AudioDeviceID device = SDL_AUDIO_DEVICE_DEFAULT_RECORDING;

  for (int32_t i = 0; i < count; i++) {
    const char *name = SDL_GetAudioDeviceName(devices[i]);
    if (name && !q_strcmp(name, s_captureDevice->string)) {
      device = devices[i];
      break;
    }
  }

  if (device == SDL_AUDIO_DEVICE_DEFAULT_RECORDING) {
    Com_Warn("Capture device \"%s\" not found, using the default\n", s_captureDevice->string);
  }

  SDL_free(devices);
  return device;
}

/**
 * @brief Opens the capture device, paused, warning once if it is unavailable.
 * @details Capture is opened on first use rather than at initialization, so that players who never
 * speak are never prompted for microphone access and never light the recording indicator.
 * @remarks Resolves s_captureDevice, whose string the main thread is free to replace, so this must
 * only be called from the main thread.
 */
bool S_OpenCapture(int32_t rate) {

  if (module.capture) {
    return true;
  }

  if (module.captureFailed) {
    return false;
  }

  if (s_captureDevice->modified) {
    s_captureDevice->modified = false;
    module.warnedSharedDevice = false;
  }

  const SDL_AudioSpec spec = {
    .format = SDL_AUDIO_S16,
    .channels = 1,
    .freq = rate,
  };

  module.capture = SDL_OpenAudioDeviceStream(S_CaptureDevice(), &spec, NULL, NULL);

  if (!module.capture) {
    Com_Warn("Failed to open capture device: %s\n", SDL_GetError());
    module.captureFailed = true;
    return false;
  }

  const char *name = SDL_GetAudioDeviceName(SDL_GetAudioStreamDevice(module.capture));

  Com_Print("Capture opened (%s)\n", name);

  S_CheckSharedDevice(name);

  return true;
}

/**
 * @brief Closes the capture device, clearing any previous failure so that it may be retried.
 */
void S_CloseCapture(void) {

  if (module.capture) {
    SDL_DestroyAudioStream(module.capture);
    module.capture = NULL;
  }

  module.captureFailed = false;
}

/**
 * @brief Starts the capture device, discarding anything buffered from last time.
 */
void S_ResumeCapture(void) {

  if (module.capture) {
    SDL_ClearAudioStream(module.capture);
    SDL_ResumeAudioStreamDevice(module.capture);
  }
}

/**
 * @brief Stops the capture device, so that the microphone is not live between transmissions.
 */
void S_PauseCapture(void) {

  if (module.capture) {
    SDL_PauseAudioStreamDevice(module.capture);
    SDL_ClearAudioStream(module.capture);
  }
}

/**
 * @brief Returns true if the capture device is open.
 */
bool S_Capturing(void) {
  return module.capture != NULL;
}

/**
 * @brief Reads exactly `len` bytes of captured audio, returning the bytes read.
 */
int32_t S_ReadCapture(void *data, int32_t len) {

  if (!module.capture || SDL_GetAudioStreamAvailable(module.capture) < len) {
    return 0;
  }

  return SDL_GetAudioStreamData(module.capture, data, len);
}

/**
 * @brief Registers the device cvars and commands.
 */
void S_InitDevices(void) {

  s_bufferFrames = Cvar_Add("s_bufferFrames", "0", CVAR_ARCHIVE | CVAR_S_DEVICE, "Playback buffer size in sample frames, or 0 to let SDL choose. Raise this if audio crackles.");
  s_captureDevice = Cvar_Add("s_captureDevice", "", CVAR_ARCHIVE, "The microphone to capture from, or empty for the system default.");

  Cmd_Add("s_captureDeviceList", S_CaptureDeviceList_f, CMD_SOUND, "List the available microphones.");
  Cmd_Add("s_playbackDeviceList", S_PlaybackDeviceList_f, CMD_SOUND, "List the available speakers.");
}
