#!/usr/bin/env bash
# Fetches the SDL3 development package into .deps/sdl3 so `make` can find it.
#
#   ./tools/fetch_deps.sh
#
# Set SDL_VERSION to pin another release.

set -euo pipefail

SDL_VERSION="${SDL_VERSION:-3.4.18}"

case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*) ;;
*)
    cat <<'EOF'
This script only automates the Windows/MinGW SDL3 download.

Install SDL3 with your package manager instead, then build with a matching
SDL3_DIR, for example:

    sudo apt install libsdl3-dev && make SDL3_DIR=/usr
    brew install sdl3 && make SDL3_DIR="$(brew --prefix sdl3)"
EOF
    exit 0
    ;;
esac

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="$ROOT/.deps/sdl3"
ARCHIVE="SDL3-devel-${SDL_VERSION}-mingw.tar.gz"
URL="https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/${ARCHIVE}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Downloading ${ARCHIVE}"
curl -fsSL -o "$TMP/$ARCHIVE" "$URL"
tar -xzf "$TMP/$ARCHIVE" -C "$TMP"

rm -rf "$DEST"
mkdir -p "$(dirname "$DEST")"
mv "$TMP/SDL3-${SDL_VERSION}/x86_64-w64-mingw32" "$DEST"

echo "SDL3 ${SDL_VERSION} installed in $DEST"
