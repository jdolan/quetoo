#!/bin/bash -x

source ~/.profile 

autoreconf -i --force
./configure --with-tests
make
cd apple
make ${MAKE_OPTIONS} bundle release image release-image
make clean
