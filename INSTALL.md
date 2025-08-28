# XScreenSaver Installation

## Quick Start

If you're building from a git repository or have modified `configure.ac`, you'll need to generate the `./configure` script first:

```bash
INTLTOOLIZE=/bin/true autoreconf -fiv
```

Then configure and build. If you want to reuse a previous configuration:

```bash
./config.status --recheck
./config.status
```

Or if you want to use a new configuration (or no previous configuration exists):

```bash
./configure --help
./configure --prefix=/usr
```

Then build and install:

```bash
make
sudo make install
make clean

xscreensaver &
xscreensaver-settings
```

## Dependencies

XScreenSaver has many compilation dependencies. The configure script will tell you what is missing, but you can install them all at once using our helper script.

### Automatic Installation (Recommended)

For Ubuntu/Debian systems:
```bash
./install-dependencies.sh
```

### Manual Installation

You will need development versions of these libraries. Append "-dev" or "-devel" to most of these:

#### Essential Build Tools
- `build-essential` - Basic compilation tools (gcc, make, etc.)

#### X11 Development Libraries
- `libx11-dev` - X11 core library
- `libxext-dev` - X11 extensions
- `libxrandr-dev` - X11 RandR extension
- `libxinerama-dev` - X11 Xinerama extension
- `libxss-dev` - X11 Screen Saver extension
- `libxt-dev` - X11 Xt library
- `libxmu-dev` - X11 Xmu library
- `libxpm-dev` - X11 Xpm library
- `libxft-dev` - X11 Xft library
- `libxrender-dev` - X11 Render extension
- `libxfixes-dev` - X11 XFixes extension
- `libxdamage-dev` - X11 Damage extension
- `libxcomposite-dev` - X11 Composite extension
- `libxcursor-dev` - X11 Cursor extension
- `libxtst-dev` - X11 Test extension
- `libxi-dev` - X11 Input extension

#### OpenGL Development Libraries
- `libgl1-mesa-dev` - OpenGL development files
- `libglu1-mesa-dev` - OpenGL Utility library
- `freeglut3-dev` - FreeGLUT development files

#### GUI Toolkit
- `libgdk-pixbuf2.0-dev` - GDK Pixbuf library
- `libgtk-3-dev` - GTK+ 3 development files

#### Multimedia Libraries
- `libavutil-dev` - FFmpeg utility library
- `libavcodec-dev` - FFmpeg codec library
- `libavformat-dev` - FFmpeg format library
- `libswscale-dev` - FFmpeg scaling library
- `libswresample-dev` - FFmpeg resampling library

#### Other Libraries
- `libxml2-dev` - XML parsing library
- `libsystemd-dev` - systemd development files

### Installation Commands by Distribution

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install -y build-essential \
    libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxss-dev \
    libxt-dev libxmu-dev libxpm-dev libxft-dev libxrender-dev \
    libxfixes-dev libxdamage-dev libxcomposite-dev libxcursor-dev \
    libxtst-dev libxi-dev \
    libgl1-mesa-dev libglu1-mesa-dev freeglut3-dev \
    libgdk-pixbuf2.0-dev libgtk-3-dev \
    libavutil-dev libavcodec-dev libavformat-dev libswscale-dev libswresample-dev \
    libxml2-dev libsystemd-dev
```

#### Fedora/RHEL/CentOS
```bash
sudo dnf install gcc make \
    libX11-devel libXext-devel libXrandr-devel libXinerama-devel libXScrnSaver-devel \
    libXt-devel libXmu-devel libXpm-devel libXft-devel libXrender-devel \
    libXfixes-devel libXdamage-devel libXcomposite-devel libXcursor-devel \
    libXtst-devel libXi-devel \
    mesa-libGL-devel mesa-libGLU-devel freeglut-devel \
    gdk-pixbuf2-devel gtk3-devel \
    ffmpeg-devel \
    libxml2-devel systemd-devel
```

#### Arch Linux
```bash
sudo pacman -S base-devel \
    libx11 libxext libxrandr libxinerama libxss \
    libxt libxmu libxpm libxft libxrender \
    libxfixes libxdamage libxcomposite libxcursor \
    libxtst libxi \
    mesa glu freeglut \
    gdk-pixbuf2 gtk3 \
    ffmpeg \
    libxml2 systemd-libs
```

**Note:** BSD systems might need `gmake` instead of `make`.

## Troubleshooting

### Common Error Messages

- `fatal error: X11/Xlib.h: No such file or directory` → Install `libx11-dev`
- `fatal error: GL/gl.h: No such file or directory` → Install `libgl1-mesa-dev`
- `fatal error: gtk/gtk.h: No such file or directory` → Install `libgtk-3-dev`
- `fatal error: libavcodec/avcodec.h: No such file or directory` → Install `libavcodec-dev`
- `fatal error: libxml/parser.h: No such file or directory` → Install `libxml2-dev`
- `fatal error: systemd/sd-bus.h: No such file or directory` → Install `libsystemd-dev`

### Development Setup

For development work, you can create symbolic links pointing back to your source directory:

```bash
make install_links
```

This makes it easier to test changes without reinstalling.

---

## Boilerplate 'Configure' Instructions

These are generic installation instructions.

The `configure` shell script attempts to guess correct values for various system-dependent variables used during compilation. It uses those values to create a `Makefile` in each directory of the package. It may also create one or more `.h` files containing system-dependent definitions. Finally, it creates a shell script `config.status` that you can run in the future to recreate the current configuration, a file `config.cache` that saves the results of its tests to speed up reconfiguring, and a file `config.log` containing compiler output (useful mainly for debugging `configure`).

If you need to do unusual things to compile the package, please try to figure out how `configure` could check whether to do them, and mail diffs or instructions to the address given in the `README` so they can be considered for the next release. If at some point `config.cache` contains results you don't want to keep, you may remove or edit it.

The file `configure.ac` is used to create `configure` by a program called `autoconf`. You only need `configure.ac` if you want to change it or regenerate `configure` using a newer version of `autoconf`.

### The simplest way to compile this package is:

1. `cd` to the directory containing the package's source code and type `./configure` to configure the package for your system. If you're using `csh` on an old version of System V, you might need to type `sh ./configure` instead to prevent `csh` from trying to execute `configure` itself.

   Running `configure` takes awhile. While running, it prints some messages telling which features it is checking for.

2. Type `make` to compile the package.

3. [There is no number three]

4. Type `make install` to install the programs and any data files and documentation.

5. You can remove the program binaries and object files from the source code directory by typing `make clean`. To also remove the files that `configure` created (so you can compile the package for a different kind of computer), type `make distclean`. There is also a `make maintainer-clean` target, but that is intended mainly for the package's developers. If you use it, you may have to get all sorts of other programs in order to regenerate files that came with the distribution.

## Compilers and Options

Some systems require unusual options for compilation or linking that the `configure` script does not know about. You can give `configure` initial values for variables by setting them in the environment. Using a Bourne-compatible shell, you can do that on the command line like this:

```bash
CC=c89 CFLAGS=-O2 LIBS=-lposix ./configure
```

Or on systems that have the `env` program, you can do it like this:

```bash
env CPPFLAGS=-I/usr/local/include LDFLAGS=-s ./configure
```

## Compiling For Multiple Architectures

You can compile the package for more than one kind of computer at the same time, by placing the object files for each architecture in their own directory. To do this, you must use a version of `make` that supports the `VPATH` variable, such as GNU `make`. `cd` to the directory where you want the object files and executables to go and run the `configure` script. `configure` automatically checks for the source code in the directory that `configure` is in and in `..`.

If you have to use a `make` that does not supports the `VPATH` variable, you have to compile the package for one architecture at a time in the source code directory. After you have installed the package for one architecture, use `make distclean` before reconfiguring for another architecture.

## Installation Names

By default, `make install` will install the package's files in `/usr/local/bin`, `/usr/local/man`, etc. You can specify an installation prefix other than `/usr/local` by giving `configure` the option `--prefix=PATH`.

You can specify separate installation prefixes for architecture-specific files and architecture-independent files. If you give `configure` the option `--exec-prefix=PATH`, the package will use PATH as the prefix for installing programs and libraries. Documentation and other data files will still use the regular prefix.

In addition, if you use an unusual directory layout you can give options like `--bindir=PATH` to specify different values for particular kinds of files. Run `configure --help` for a list of the directories you can set and what kinds of files go in them.

If the package supports it, you can cause programs to be installed with an extra prefix or suffix on their names by giving `configure` the option `--program-prefix=PREFIX` or `--program-suffix=SUFFIX`.

## Optional Features

Some packages pay attention to `--enable-FEATURE` options to `configure`, where FEATURE indicates an optional part of the package. They may also pay attention to `--with-PACKAGE` options, where PACKAGE is something like `gnu-as` or `x` (for the X Window System). The `README` should mention any `--enable-` and `--with-` options that the package recognizes.

For packages that use the X Window System, `configure` can usually find the X include and library files automatically, but if it doesn't, you can use the `configure` options `--x-includes=DIR` and `--x-libraries=DIR` to specify their locations.

## Specifying the System Type

There may be some features `configure` can not figure out automatically, but needs to determine by the type of host the package will run on. Usually `configure` can figure that out, but if it prints a message saying it can not guess the host type, give it the `--host=TYPE` option. TYPE can either be a short name for the system type, such as `sun4`, or a canonical name with three fields:

```
CPU-COMPANY-SYSTEM
```

See the file `config.sub` for the possible values of each field. If `config.sub` isn't included in this package, then this package doesn't need to know the host type.

If you are building compiler tools for cross-compiling, you can also use the `--target=TYPE` option to select the type of system they will produce code for and the `--build=TYPE` option to select the type of system on which you are compiling the package.

## Sharing Defaults

If you want to set default values for `configure` scripts to share, you can create a site shell script called `config.site` that gives default values for variables like `CC`, `cache_file`, and `prefix`. `configure` looks for `PREFIX/share/config.site` if it exists, then `PREFIX/etc/config.site` if it exists. Or, you can set the `CONFIG_SITE` environment variable to the location of the site script. A warning: not all `configure` scripts look for a site script.

## Operation Controls

`configure` recognizes the following options to control how it operates.

- `--cache-file=FILE` - Use and save the results of the tests in FILE instead of `./config.cache`. Set FILE to `/dev/null` to disable caching, for debugging `configure`.

- `--help` - Print a summary of the options to `configure`, and exit.

- `--quiet`, `--silent`, `-q` - Do not print messages saying which checks are being made. To suppress all normal output, redirect it to `/dev/null` (any error messages will still be shown).

- `--srcdir=DIR` - Look for the package's source code in directory DIR. Usually `configure` can determine that directory automatically.

- `--version` - Print the version of Autoconf used to generate the `configure` script, and exit.

`configure` also accepts some other, not widely useful, options.
