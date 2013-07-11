#!/bin/bash
set -x

source ~/.profile 

autoreconf -i --force
./configure --with-tests --with-master
make
make DESTDIR=/Users/q2wbuild/jenkins/quake2world-result install
