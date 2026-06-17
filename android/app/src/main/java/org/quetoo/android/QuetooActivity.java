/*
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
 */

package org.quetoo.android;

import org.libsdl.app.SDLActivity;

/**
 * The Quetoo Android entry activity.
 *
 * <p>Extends SDL3's {@link SDLActivity}, which owns the window/surface, the
 * GL/EGL context, audio, input and the app lifecycle. SDLActivity loads the
 * native libraries returned by {@link #getLibraries()} (in order) via
 * {@code System.loadLibrary}, then calls {@code SDL_main} in the last one.</p>
 *
 * <p>The engine's {@code int main(int, char**)} (src/main/main.c) is renamed to
 * {@code SDL_main} by {@code <SDL3/SDL_main.h>} when the Android client target
 * is compiled, so the native library that exports it MUST be named
 * {@code "main"} (-> {@code libmain.so}). That naming is the contract with the
 * CMake build in {@code android/CMakeLists.txt}.</p>
 */
public class QuetooActivity extends SDLActivity {

    /**
     * Native libraries to load, in dependency order.
     *
     * <p>SDL3 first (provides the JNI bridge + {@code SDL_main} dispatch), then
     * the Quetoo engine library {@code libmain.so} (loaded last; SDL runs its
     * {@code SDL_main}).</p>
     *
     * <p>This assumes the engine is linked into a single {@code libmain.so}
     * (engine + statically-linked deps: OpenAL-soft, PhysicsFS, libcurl, the
     * qglib shim, etc.). The {@code game.so}/{@code cgame.so} modules are NOT
     * listed here: they are {@code dlopen}'d at runtime by the engine from the
     * APK's native lib dir (see PORTING.md §4), not loaded by SDL.</p>
     *
     * <p>If a dependency is instead shipped as its own shared object, add it to
     * this array BEFORE {@code "main"} (e.g. {@code "openal", "main"}).</p>
     */
    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL3",
            // "SDL3_image",   // uncomment if the engine links SDL3_image as a shared lib
            // "openal",       // uncomment if OpenAL-soft is shipped as libopenal.so
            "main",            // libmain.so — Quetoo engine; exports SDL_main
        };
    }

    /**
     * Extra command-line arguments passed to the engine's {@code main()}.
     *
     * <p>Returning an empty array launches the client normally. Use this to wire
     * launch-time {@code +commands} or cvars if needed (e.g. for debugging).</p>
     */
    @Override
    protected String[] getArguments() {
        return new String[] {};
    }
}
