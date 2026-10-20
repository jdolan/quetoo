[![Build Status](https://github.com/jdolan/quetoo/actions/workflows/build.yml/badge.svg)](https://github.com/jdolan/quetoo/actions/workflows/build.yml)
[![GPLv2 License](https://img.shields.io/badge/license-GPL%20v2-brightgreen.svg)](https://opensource.org/licenses/GPL-2.0)
# Quetoo

![Quetoo](https://raw.githubusercontent.com/jdolan/quetoo/main/quetoo-edge.jpg)

## Overview

[_Quetoo_](https://quetoo.org) ("Q2") is a free first person shooter for Mac, PC and Linux. Our goal is to bring the fun of oldschool deathmatch to a new generation of gamers. We're pwning nubz, one rail slug at a time.

## Features

 * Deathmatch, Instagib and Rocket Arena gameplay, each with a team variant, plus Capture the Flag
 * Wicked good bots for online and offline practice
 * Single-player ladders against increasingly difficult bots
 * Race mode, with records and replays
 * Grappling hook and techs, enabled by default in Capture the Flag
 * In-game voice chat
 * High quality remakes of id Software's legendary _Quake_ series deathmatch levels, as well as many originals
 * Realtime lighting with soft shadowmapping
 * Steep parallax occlusion mapping with self-shadowing
 * Dramatically improved artwork pipeline with in-game editor for level artists
 * TrenchBroom support
 * 100% free to download, play and modify. Source code licensed under GPL v2, artwork under Creative Commons.

## Downloads

Releases of _Quetoo_ for all platforms are available on the [GitHub Releases](https://github.com/jdolan/quetoo/releases) page:

 * **macOS**: a signed and notarized disk image for Apple Silicon
 * **Windows**: a zip archive for x86_64
 * **Linux**: `.tar.gz` archives, and `.deb` and `.rpm` client and server packages, for x86_64 and aarch64

On first launch, _Quetoo_ will automatically download and install the game data.

### Windows

Windows SmartScreen may block _Quetoo_ on first launch. Click **More info**, then **Run anyway** to proceed.

## Community

Looking for a game? Join the official [Quetoo Discord](https://discord.gg/unb9U4b) server.

## Compiling

Compiling _Quetoo_ is only recommended for developers and modders. Supported platforms and targets include GNU Linux, BSD, macOS, and Microsoft Visual Studio.

The following dependencies are required:

 * [SDL3](https://github.com/jdolan/SDL/tree/ObjectivelyGPU), with SDL3_image and SDL3_ttf
 * [Objectively](https://github.com/jdolan/Objectively/)
 * [ObjectivelyGPU](https://github.com/jdolan/ObjectivelyGPU/)
 * [ObjectivelyMVC](https://github.com/jdolan/ObjectivelyMVC/)
 * [PhysicsFS](https://icculus.org/physfs/)
 * [OpenAL](https://www.openal.org/)
 * [Opus](https://opus-codec.org/)
 * [libsndfile](https://libsndfile.github.io/libsndfile/)
 * [ncurses](https://www.gnu.org/software/ncurses/)
 * [Check](https://libcheck.github.io/check/), for the unit tests only

_Quetoo_ requires a fork of SDL3 that adds the SDL_gpu query API. Stock SDL3 builds, but occlusion queries silently do nothing. SDL3_image and SDL3_ttf must be built from source against the fork. See ObjectivelyGPU's [installation guide](https://github.com/jdolan/ObjectivelyGPU/blob/main/Documentation/install.md) for the full procedure.

Quetoo builds with GNU Autotools. To build it, run the following:

    autoreconf -i
    ./configure [--with-tests]
    make -j8 && sudo make install

## Installing

To have a working game, you must install the game data using `git`:

    git clone https://github.com/jdolan/quetoo-data.git
    sudo ln -s $(pwd)/quetoo-data/target /usr/local/share/quetoo

## Support

Support is available via [Discord](https://discord.gg/unb9U4b).
