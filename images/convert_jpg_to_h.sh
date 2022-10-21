#!/bin/sh

infile="$1"
outfile=../include/screen/bitcoin-logo.jpg.h

convert "$infile" -resize 200x200 -depth 8 -size 200x200 bitcoin-logo.jpg

echo "static const unsigned char logo_jpg[] PROGMEM  = {" > "$outfile"

od -v -t x1 -A n bitcoin-logo.jpg | sed "s/ /,0x/g" | tail -c +2 >> "$outfile"

echo "};" >> "$outfile"
