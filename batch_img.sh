#!/bin/bash

DEPDIR=~/x86_64_poios
SRCDIR=$DEPDIR/materials/images
DSTDIR=$DEPDIR/articles/images
# HEIGHT=1200
DEFAULT_WIDTH=1600
# DENSITY=137.8
DENSITY=350

imglst=$(grep '^//image' *.re | cut -d '[' -f 2 | cut -d ']' -f1 | sort | uniq)

for img in $imglst; do
	src=$(echo $SRCDIR/$img.*)
	# name=$(echo $src | rev | cut -d '/' -f 1 | rev)
	name=$(basename $src)

	# 出力画像サイズ確定
	nosuff=$(echo $name | rev | cut -d'.' -f2- | rev)
	case $nosuff in
	'020_fill_qemu')
		WIDTH=1200
		;;
	'020_fill_lenovo')
		WIDTH=1200
		;;
	'022_font')
		WIDTH=1000
		;;
	'030_keyinput_polling')
		WIDTH=1000
		;;
	'050_fs_read_text')
		WIDTH=1000
		;;
	'050_rofs_img')
		WIDTH=1400
		;;
	'052_fs_create_fs')
		WIDTH=1000
		;;
	'060_qemu')
		WIDTH=400
		;;
	'060_lenovo')
		WIDTH=800
		;;
	'061_iv_view_a_image')
		WIDTH=1400
		;;
	'062_iv_ls')
		WIDTH=400
		;;
	'063_iv_image_viewer_1')
		WIDTH=1400
		;;
	'063_iv_image_viewer_2')
		WIDTH=1400
		;;
	*)
		WIDTH=${DEFAULT_WIDTH}
		;;
	esac

	# $dstのsuffix確定
	suff=$(echo $name | rev | cut -d'.' -f1 | rev)
	if [ "$suff" = "svg" ]; then
		dst=$DSTDIR/$(echo $name | rev | cut -d'.' -f2- | rev).png
	else
		dst=$DSTDIR/$name
	fi
	# echo "src=$src, suff=$suff, dst=$dst"

	com="convert $src -page $(identify $src | cut -d ' ' -f 3)+0+0 -resize ${WIDTH}x$HEIGHT -units PixelsPerInch -resample 350 -type GrayScale -resize ${WIDTH}x$HEIGHT $dst"

	echo $com
	$com
done
