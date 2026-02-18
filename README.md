# XScreenSaver

A collection of free screen savers for X11, macOS, iOS and Android

**By Jamie Zawinski and many others**

**Website:** https://www.jwz.org/xscreensaver/

---

This is the XScreenSaver source code distribution. It is strongly recommended that you install a binary release rather than trying to compile it yourself. Executables are available for almost all platforms, including macOS, iOS, and Android. See the XScreenSaver web site for details.

## Quick Start

### For Unix systems with X11:

```bash
./configure --help
./configure --prefix=/usr
make
sudo make install

xscreensaver &
xscreensaver-settings
```

### Dependencies

There are many compilation dependencies. The configure script will tell you what is missing. For a complete list of dependencies and installation instructions, see [INSTALL.md](INSTALL.md).

**Quick start for Ubuntu/Debian:**
```bash
./install-dependencies.sh
./configure --prefix=/usr
make
```

At the least, you will need development versions of these libraries. Append "-dev" or "-devel" to most of these:

- perl attr pkg-config gettext intltool libx11 libxext libxi libxt
- libxft libxinerama libxrandr libxxf86vm libgl libglu libgle
- libgtk-3 libgdk-pixbuf2.0 libjpeg libxml2 libpam libwayland
- libsystemd elogind

BSD systems might need `gmake` instead of `make`.

### For macOS or iOS:

See [OSX/README](OSX/README). Use the included Xcode project.

### For Android:

See [android/README](android/README).

## Interested in writing a new screen saver?

See the [README.hacking](README.hacking) file.

## Documentation

- **[INSTALL.md](INSTALL.md)** - Complete installation instructions and dependency information
- **[CHANGELOG.md](CHANGELOG.md)** - Complete version history and changelog
- **[README.hacking](README.hacking)** - Guide for writing new screen savers
