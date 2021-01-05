[![Linux Build Status](http://ci.quetoo.org/buildStatus/icon?job=Quetoo-Linux-x86_64-master)](http://ci.quetoo.org/job/Quetoo-Linux-x86_64-master/)
[![Windows Build Status](https://ci.appveyor.com/api/projects/status/647d2fdblb63rkhy?svg=true)](https://ci.appveyor.com/project/Paril/quetoo)
[![GPLv2 License](https://img.shields.io/badge/license-GPL%20v2-brightgreen.svg)](https://opensource.org/licenses/GPL-2.0)
![This software is BETA](https://img.shields.io/badge/development_stage-BETA-yellowgreen.svg)

# Quetoo BETA

![Quetoo BETA](http://quetoo.org/files/15385369_1245001622212024_7988137002503923923_o.jpg)

## Overview

[_Quetoo_](http://quetoo.org) is a Free first person shooter for Mac, PC and Linux. Our goal is to bring the fun of oldschool deathmatch to a new generation of gamers.

## Features

 * Deathmatch, Capture, Instagib, Duel and Rocket Arena gameplay modes
 * Teams Play and Match Mode support
 * High quality remakes of id Software's legendary _Quake II_ deathmatch levels, as well as originals
 * Original sounds and game music by <a href="http://rolandshaw.wordpress.com/">Roland Shaw</a> and <a href="http://anthonywebbmusic.com/">Anthony Webb</a>.
 * 100% free to download, play and modify. See our <a href="http://quetoo.org/books/documentation/licensing">licensing</a> page.

## Downloads

Preview releases of _Quetoo_ for all platforms are available for download on the [project website](http://quetoo.org/pages/downloads). Installation instructions are available there as well.

## Community

Looking for a game? Join the official [Quetoo Discord](https://discord.gg/unb9U4b) server.

## Compiling

Compiling _Quetoo_ is only recommended for advanced users. Supported platforms and targets include GNU Linux, BSD, Apple OS X, MinGW Cross Compile, and Microsoft Visual Studio. See [Installation and Maintenance](http://quetoo.org/books/documentation/installation-and-maintenance).

The following dependencies are required:

 * [ObjectivelyMVC](https://github.com/jdolan/ObjectivelyMVC/)
 * [PhysicsFS](https://icculus.org/physfs/)
 * [OpenAL](https://www.openal.org/)
 * [libsndfile](http://mega-nerd.com/libsndfile/)
 * [glib2](https://developer.gnome.org/glib/)
 * [ncurses](https://www.gnu.org/software/ncurses/)
 * [libxml2](http://xmlsoft.org/)

Quetoo builds with GNU Autotools. To build it, run the following:

    autoreconf -i
    ./configure [--with-tests --with-master]
    make && sudo make install

## Installing

To have a working game, you must install the game data using `git`:

    git clone https://github.com/jdolan/quetoo-data.git
    sudo ln -s quetoo-data/target /usr/local/share/quetoo

More information on hacking on _Quetoo_ is available [on the project website](http://quetoo.org/books/documentation/developing-and-modding).

## Support

Support is available via [Discord](https://discord.gg/unb9U4b).
