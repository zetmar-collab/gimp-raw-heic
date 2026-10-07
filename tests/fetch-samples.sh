#!/bin/sh
# Download one RAW sample per required format from raw.pixls.us (CC0 samples).
# Usage: tests/fetch-samples.sh [destdir]
set -u
DEST=${1:-"$(dirname "$0")/../samples"}
mkdir -p "$DEST"
BASE=https://raw.pixls.us/data
# make|model|extension
LIST='Canon|EOS 6D|CR2
Canon|EOS R10|CR3
Nikon|D750|NEF
Sony|ILCE-7M3|ARW
Fujifilm|X-T3|RAF
Google|Pixel 4a|dng
Apple|iPhone 12 Pro|dng
Panasonic|DC-G9|RW2
Olympus|E-M10 Mark II|ORF
Pentax|K-70|PEF'
echo "$LIST" | while IFS='|' read -r make model ext; do
  m=$(printf '%s' "$make" | sed 's/ /%20/g'); d=$(printf '%s' "$model" | sed 's/ /%20/g')
  url="$BASE/$m/$d/"
  f=$(curl -sfL "$url" | grep -oiE "href=\"[^\"/]+\.$ext\"" | head -1 | sed 's/href="//;s/"$//')
  if [ -z "$f" ]; then echo "NOTFOUND $make $model ($url)"; continue; fi
  out="$DEST/$(printf '%s' "$make-$model" | tr ' ' '_').$(echo "$ext" | tr 'A-Z' 'a-z')"
  [ -s "$out" ] || curl -sfL "$url$f" -o "$out"
  echo "OK $out $(wc -c < "$out")"
done
