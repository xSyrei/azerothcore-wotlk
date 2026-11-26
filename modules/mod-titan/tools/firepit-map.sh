#!/usr/bin/env bash
# The Firepit's map (760) is Trial of the Champion's (650) under another name: its terrain, collision and navigation
# files are copies of map 650's. Run by docker-compose's ac-firepit-map service on every start (it overwrites them),
# or by hand: firepit-map.sh <the server's data dir>.
set -euo pipefail
DATA="${1:-/azerothcore/env/dist/data}"
SOURCE=650
TARGET=760
copied=0
for dir in maps vmaps mmaps; do
  [ -d "$DATA/$dir" ] || continue
  for file in "$DATA/$dir/$SOURCE"*; do
    [ -e "$file" ] || continue
    name="$(basename "$file")"
    cp -f "$file" "$DATA/$dir/$TARGET${name#"$SOURCE"}"
    copied=$((copied + 1))
  done
done
echo "The Firepit: $copied file(s) of map $SOURCE copied as map $TARGET"
[ "$copied" -gt 0 ]
