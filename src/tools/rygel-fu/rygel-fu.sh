#!/bin/bash

check_layer_sizes() {

  diffusemap=$1
  dw=$(identify -format %w $diffusemap)
  dh=$(identify -format %h $diffusemap)

  normalmap=${diffusemap/.jpg/_norm.jpg}
  test -f ${normalmap} && {
    nw=$(identify -format %w ${normalmap})
    nh=$(identify -format %h ${normalmap})

    if [[ $dw != $nw || $dh != $nh ]]; then
      echo $(basename $diffusemap) $dw x $dh, $(basename $normalmap) $nw x $nh
    fi
  }

  glossmap=${diffusemap/.jpg/_gloss.jpg}
  test -f ${glossmap} && {
    gw=$(identify -format %w ${glossmap})
    gh=$(identify -format %h ${glossmap})

    if [[ $dw != $gw || $dh != $gh ]]; then
      echo $(basename $diffusemap) $dw x $dh, $(basename $glossmap) $gw x $gh
    fi
  }
}

for i in $1/*.jpg; do
  case $i in
    *_norm.jpg)
      ;;
    *_gloss.jpg)
      ;;
    *_luma.jpg)
      ;;
    *)
      check_layer_sizes $i
      ;;
  esac
done