#!/bin/bash
set -e

source ~/.profile 

pwd

autoreconf -i --force
./configure --with-tests --with-master
make
make DESTDIR=/Users/q2wbuild/jenkins/quake2world-result install
