# Quetoo BETA
![Quetoo BETA](http://farm8.staticflickr.com/7052/6840396962_e01802d3f9_c.jpg)

## Overview

[_Quetoo_](http://quetoo.org) is a Free, standalone first person shooter video game based on id Tech2. Our goal is to bring the fun of oldschool deathmatch to a more contemporary platform, and perhaps to a new generation of gamers. _Quetoo_ is multiplayer-only. There is no single-player or offline gameplay mode. If you have no Internet and no friends, _Quetoo_ might not be for you. Some of _Quetoo_'s features include:

 * Deathmatch, Capture, Instagib, and Rocket Arena gameplay modes right out of the box.
 * Teams Play and Match Mode support, with Warmup and Ready functionality.
 * High quality original levels and remakes of _Quake_ series classics.
 * Original sounds and game music by <a href="http://rolandshaw.wordpress.com/">Roland Shaw</a>.
 * 100% free to download, play and modify. See our <a href="http://quetoo.org/books/documentation/licensing">licensing</a> page.

## Downloads

Preview releases of _Quetoo_ for all platforms are available for download on the [project website](http://quetoo.org/pages/downloads). Installation instructions are available there as well.

## Compiling

Compilation of _Quetoo_ is only recommended for users running GNU/Linux or MacOS X. Windows users should consider using our cross-compiled snapshots. For more information, see [Installation and Maintenance](http://quetoo.org/books/documentation/installation-and-maintenance).

Quetoo builds with GNU Autotools. To build it, run the following:

    autoreconf -i
    ./configure [--with-tests --with-master --without-tools --without-client]
    make -j3 && sudo make install

## Installing

To have a working game, you must install the game data. You have two options: `git` or `rsync`. If you wish to work on the `experimental` branch, or if you wish to modify the game data, you should use `git`:

    git clone https://github.com/jdolan/quetoo-data.git
    sudo ln -s quetoo-data/target /usr/local/share/quetoo
    
If you're working on `master` and have no plans to modify the game data, you can simply run `sudo make rsync-data` from the source code working copy.

More information on hacking on _Quetoo_ is available [on the project website](http://quetoo.org/books/documentation/developing-and-modding).

## Support
 * The IRC channel for this project is *#quetoo* on *irc.freenode.net*
