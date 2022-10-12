#!/bin/sh

infile=bitcoin_blue.png
infile_small="$infile"
outfile=../include/screen/logo.png.h

identify "$infile_small"


echo "static const unsigned char logo_png[] PROGMEM  = {" > "$outfile"

od -v -t x1 -A n "$infile_small" | sed "s/ /,0x/g" | tail -c +2 >> "$outfile"

echo "};" >> "$outfile"
