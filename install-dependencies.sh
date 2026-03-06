#!/bin/bash

# xscreensaver build dependencies installer (Debian/Ubuntu family)

set -euo pipefail

if ! command -v apt >/dev/null 2>&1; then
  echo "Error: this installer currently supports Debian/Ubuntu (apt) systems." >&2
  exit 1
fi

if [[ ${EUID:-$(id -u)} -eq 0 ]]; then
  SUDO=()
else
  SUDO=(sudo)
fi

echo "Installing xscreensaver build dependencies..."

package_available() {
  local pkg="$1"
  apt-cache show "$pkg" >/dev/null 2>&1
}

choose_first_available() {
  local pkg
  for pkg in "$@"; do
    if package_available "$pkg"; then
      echo "$pkg"
      return 0
    fi
  done
  return 1
}

PKGS=(
  build-essential
  autoconf
  automake
  gettext
  intltool
  libpam0g-dev
  libx11-dev
  libxext-dev
  libxrandr-dev
  libxinerama-dev
  libxss-dev
  libxt-dev
  libxmu-dev
  libxpm-dev
  libxft-dev
  libxrender-dev
  libxfixes-dev
  libxdamage-dev
  libxcomposite-dev
  libxcursor-dev
  libxtst-dev
  libxi-dev
  libgl1-mesa-dev
  libglu1-mesa-dev
  freeglut3-dev
  libgtk-3-dev
  libavutil-dev
  libavcodec-dev
  libavformat-dev
  libswscale-dev
  libswresample-dev
  libxml2-dev
  libsystemd-dev
)

if pixbuf_pkg="$(choose_first_available \
  libgdk-pixbuf-2.0-dev \
  libgdk-pixbuf2.0-dev \
  libgdk-pixbuf-xlib-2.0-dev)"; then
  PKGS+=("$pixbuf_pkg")
else
  echo "Error: no supported GDK Pixbuf development package was found." >&2
  exit 1
fi

echo "Selected GDK Pixbuf package: $pixbuf_pkg"

"${SUDO[@]}" apt update
"${SUDO[@]}" apt install -y "${PKGS[@]}"

# Some git checkouts may lack these Automake helper scripts.
if [[ ! -f config.guess || ! -f config.sub ]]; then
  am_libdir="$(automake --print-libdir)"
  [[ -f config.guess ]] || cp "$am_libdir/config.guess" ./config.guess
  [[ -f config.sub   ]] || cp "$am_libdir/config.sub"   ./config.sub
fi

echo "All dependencies installed successfully."
echo "Next steps:"
echo "  INTLTOOLIZE=/bin/true autoreconf -fiv"
echo "  ./configure --prefix=/usr"
echo "  make"
