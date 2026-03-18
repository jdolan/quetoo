[![Build Status](https://github.com/jdolan/quetoo/actions/workflows/build.yml/badge.svg)](https://github.com/jdolan/quetoo/actions/workflows/build.yml)
[![GPLv2 License](https://img.shields.io/badge/license-GPL%20v2-brightgreen.svg)](https://opensource.org/licenses/GPL-2.0)
![This software is BETA](https://img.shields.io/badge/development_stage-BETA-yellowgreen.svg)

# Quetoo BETA

![Quetoo BETA!](https://raw.githubusercontent.com/jdolan/quetoo/main/quetoo-edge.jpg)

## Overview

[_Quetoo_](http://quetoo.org) ("Q2") is a free first person shooter for Mac, PC and Linux. Our goal is to bring the fun of oldschool deathmatch to a new generation of gamers. We're pwning nubz, one rail slug at a time.

## Features

 * Deathmatch, Team DM, Capture the Flag, Instagib and Rocket Arena gameplay modes
 * High quality remakes of id Software's legendary _Quake_ series deathmatch levels, as well as many originals
 * Realtime lighting with soft shadowmapping
 * Bump mapping with parallax occlusion mapping and self-shadowing
 * Volumetric fog
 * Dramatically improved artwork pipeline with in-game editor for level artists
 * TrenchBroom support
 * 100% free to download, play and modify. See our <a href="http://quetoo.org/books/documentation/licensing">licensing</a> page.

## Downloads

Preview releases of _Quetoo_ for all platforms are available for download on the [project website](http://quetoo.org/pages/downloads). Installation instructions are available there as well.

## Community

Looking for a game? Join the official [Quetoo Discord](https://discord.gg/unb9U4b) server.

## Compiling

Compiling _Quetoo_ is only recommended for developers and mappers. Supported platforms and targets include GNU Linux, BSD, macOS, MinGW Cross Compile, and Microsoft Visual Studio. See [Developing and Modding](http://quetoo.org/books/documentation/developing-and-modding) for further details.

The following dependencies are required:

 * [ObjectivelyMVC](https://github.com/jdolan/ObjectivelyMVC/)
 * [PhysicsFS](https://icculus.org/physfs/)
 * [OpenAL](https://www.openal.org/)
 * [libsndfile](http://mega-nerd.com/libsndfile/)
 * [glib2](https://developer.gnome.org/glib/)
 * [ncurses](https://www.gnu.org/software/ncurses/)

Quetoo builds with GNU Autotools. To build it, run the following:

    autoreconf -i
    ./configure [--with-tests --with-master]
    make && sudo make install

## Installing

To have a working game, you must install the game data using `git`:

    git clone https://github.com/jdolan/quetoo-data.git
    sudo ln -s $(pwd)/quetoo-data/target /usr/local/share/quetoo

## Support

Support is available via [Discord](https://discord.gg/unb9U4b).
