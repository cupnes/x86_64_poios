#!/bin/bash

DEPDIR=~/x86_64_poios
SRCDIR=$DEPDIR/materials/images
DSTDIR=$DEPDIR/articles/images
# HEIGHT=1200
WIDTH=1600
# DENSITY=137.8
DENSITY=350

imglst=$(grep '^//image' *.re | cut -d '[' -f 2 | cut -d ']' -f1 | sort | uniq)

for img in $imglst; do
	# echo "img: $img"
	src=$(echo $SRCDIR/$img.*)
	# echo "src: $src"
	name=$(echo $src | rev | cut -d '/' -f 1 | rev)
	dst=$DSTDIR/$name
	# echo "dst: $dst"

	com="convert $src -page $(identify $src | cut -d ' ' -f 3)+0+0 -resize ${WIDTH}x$HEIGHT -units PixelsPerInch -resample 350 -type GrayScale -resize ${WIDTH}x$HEIGHT $dst"

	echo $com
	$com
done
