#!/usr/bin/env sh

set -eu

IWAD_PATH=${IWAD_PATH:-}
PWAD_PATH=${PWAD_PATH:-}

if [ -z "${IWAD_PATH}" ] && [ $# -ge 1 ]; then
  IWAD_PATH=$1
  shift
fi

if [ -z "${IWAD_PATH}" ]; then
  echo "Usage: $0 /path/to/DOOM.WAD [extra args]" >&2
  echo "Environment: IWAD_PATH, PWAD_PATH (optional)" >&2
  exit 1
fi

BIN_DIR=${BIN_DIR:-"./build-make/bin"}
BIN_PATH="$BIN_DIR/linuxdoom"

if [ ! -x "$BIN_PATH" ]; then
  echo "Error: $BIN_PATH not found or not executable" >&2
  exit 1
fi

tmpdir=""
cleanup() {
  if [ -n "${tmpdir}" ] && [ -d "${tmpdir}" ]; then
    rm -rf "${tmpdir}"
  fi
}
trap cleanup EXIT INT TERM

iwad_dir=$(dirname "$IWAD_PATH")
iwad_base=$(basename "$IWAD_PATH")

if [ "$iwad_base" = "doom.wad" ] || [ "$iwad_base" = "doom2.wad" ] || [ "$iwad_base" = "doom1.wad" ] || [ "$iwad_base" = "doomu.wad" ]; then
  export DOOMWADDIR="$iwad_dir"
else
  tmpdir=$(mktemp -d)
  ln -s "$IWAD_PATH" "$tmpdir/doom.wad"
  export DOOMWADDIR="$tmpdir"
fi

set -- "$BIN_PATH" "$@"

if [ -n "${PWAD_PATH}" ]; then
  set -- "$@" -file "$PWAD_PATH"
fi

exec "$@"
