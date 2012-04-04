#!/bin/bash

QUAKE2WORLD_HOME=$(dirname "$0")
QUAKE2WORLD_HOME=${QUAKE2WORLD_HOME/Update.app*/Quake2World.app}

echo
echo "Updating $QUAKE2WORLD_HOME.."
echo

APPLE=rsync://quake2world.net/quake2world-apple/x86_64
rsync -rzhP --delete $APPLE/MacOS/ ${QUAKE2WORLD_HOME}/Contents/MacOS || exit 1
rsync -rzhP --delete $APPLE/lib/ ${QUAKE2WORLD_HOME}/Contents/lib || exit 1

DATA=rsync://quake2world.net/quake2world/default/
rsync -rzhP --delete $DATA ${QUAKE2WORLD_HOME}/Contents/Resources/default || exit 1

echo
echo "Update complete."
echo
