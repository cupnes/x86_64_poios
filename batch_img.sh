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
	src=$(echo $SRCDIR/$img.*)
	# name=$(echo $src | rev | cut -d '/' -f 1 | rev)
	name=$(basename $src)
	suff=$(echo $name | rev | cut -d'.' -f1 | rev)
	if [ "$suff" = "svg" ]; then
		dst=$DSTDIR/$(echo $name | rev | cut -d'.' -f2- | rev).png
	else
		dst=$DSTDIR/$name
	fi
	echo "src=$src, suff=$suff, dst=$dst"

	com="convert $src -page $(identify $src | cut -d ' ' -f 3)+0+0 -resize ${WIDTH}x$HEIGHT -units PixelsPerInch -resample 350 -type GrayScale -resize ${WIDTH}x$HEIGHT $dst"

	echo $com
	$com
done
