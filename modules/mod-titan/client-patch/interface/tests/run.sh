#!/bin/sh
# Offline checks for the custom character creation screen (needs python3 and lua5.1):
#  - every anchor points at a region the client has already created at that point;
#  - a full creation run (race, gender, customize, name, create, paid recustomization) against a
#    mock of the 3.3.5a login API, with widgets that reject methods the client does not have.
set -e
DIR=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
python3 "$DIR/tests/check_anchors.py" "$DIR/GlueXML/CharacterCreate.xml"
python3 "$DIR/tests/xml_to_mocks.py" "$DIR/GlueXML/CharacterCreate.xml" "$TMP/widgets.lua"
lua5.1 "$DIR/tests/charactercreate_flow.lua" "$DIR/GlueXML" "$TMP" "$DIR/GlueXML/GlueStrings.lua"
