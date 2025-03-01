# hacks/glx/Makefile.in --- xscreensaver, Copyright © 1999-2024 Jamie Zawinski.
# the `../../configure' script generates `hacks/glx/Makefile' from this file.

.SUFFIXES:
.SUFFIXES: .c .o

srcdir		= .

top_srcdir	= ../..
top_builddir	= ..

install_prefix	=
prefix		= /usr/local
exec_prefix	= ${prefix}
bindir		= ${exec_prefix}/bin
datarootdir	= ${prefix}/share
datadir		= ${datarootdir}
mandir		= ${datarootdir}/man
libexecdir	= ${exec_prefix}/libexec
mansuffix	= 6
manNdir		= $(mandir)/man$(mansuffix)

HACKDIR		= ${libexecdir}/xscreensaver
HACK_CONF_DIR	= ${datadir}/xscreensaver/config

MINGW_PREFIX = x86_64-w64-mingw32-
WINDOWS_INCLUDE = -I../../hacks -I../../utils -I/usr/x86_64-w64-mingw32/include
WINDOWS_LIBS = -L/usr/x86_64-w64-mingw32/lib -lopengl32 -lglut -lglu32 -lwinmm

# SDL3 paths for Linux build
SDL3_LIBS = -L/usr/local/lib -lSDL3 -lGL
SDL3_INCLUDE = -I/usr/local/include

# SDL3 paths for Windows cross-compile
WIN_SDL3_LIBS = -L$(WIN_SDL3_PATH)/lib -lSDL3 -lSDL3_ttf -lopengl32 -lglu32 -lmingw32 -mwindows -lm

CC		= gcc -std=gnu99 -pedantic -Wall -Wnested-externs -Wstrict-prototypes -Wmissing-prototypes -Wno-overlength-strings
CFLAGS		= -g -O2
LDFLAGS		=  -L${exec_prefix}/lib
DEFS		= -DSTANDALONE -DUSE_GL -DHAVE_CONFIG_H
PERL		= /usr/bin/perl

THREAD_LIBS	= -lpthread
THREAD_CFLAGS	= -pthread

DEPEND		= makedepend

SHELL		= /bin/sh
INSTALL		= /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_SCRIPT  = ${INSTALL}
INSTALL_DATA	= ${INSTALL} -m 644
INSTALL_DIRS	= ${INSTALL} -d

X_PRE_LIBS	=  -lSM -lICE
X_EXTRA_LIBS	=
XFT_LIBS	= -lXft

# Note: see comment in ../driver/Makefile.in for explanation of X_LIBS, etc.
#
HACK_PRE	= $(LIBS) $(X_LIBS)
HACK_POST     = $(X_PRE_LIBS) $(XFT_LIBS) -lXt -lX11 -lXext $(X_EXTRA_LIBS) -lm
HACK_LIBS	= $(HACK_PRE) -lavutil -lavcodec -lavformat -lswscale -lswresample    $(HACK_POST)
PNG_LIBS	= $(HACK_PRE) -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0  -lavutil -lavcodec -lavformat -lswscale -lswresample    $(HACK_POST)
JPEG_LIBS	= -ljpeg
XLOCK_LIBS	= $(HACK_LIBS)
TEXT_LIBS	= -lutil

HACK_SRC	= $(srcdir)/..
HACK_BIN	= ..
UTILS_SRC	= $(HACK_SRC)/../utils
JWXYZ_SRC	= $(HACK_SRC)/../jwxyz
UTILS_BIN	= $(HACK_BIN)/../utils
JWXYZ_BIN	= $(HACK_BIN)/../jwxyz

DRIVER_SRC	= $(srcdir)/../../driver
DRIVER_BIN	= ../../driver

INCLUDES_1	= -I. -I$(srcdir) -I$(UTILS_SRC) -I$(JWXYZ_SRC) -I$(HACK_SRC) -I$(HACK_BIN) -I../..
INCLUDES	= $(INCLUDES_1)   -I/usr/include/gtk-3.0 -I/usr/include/pango-1.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/harfbuzz -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/libmount -I/usr/include/blkid -I/usr/include/fribidi -I/usr/include/cairo -I/usr/include/pixman-1 -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/x86_64-linux-gnu -I/usr/include/webp -I/usr/include/gio-unix-2.0 -I/usr/include/atk-1.0 -I/usr/include/at-spi2-atk/2.0 -I/usr/include/at-spi-2.0 -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -I/usr/include/libxml2 -pthread  -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/libpng16 -I/usr/include/x86_64-linux-gnu -I/usr/include/webp -I/usr/include/libmount -I/usr/include/blkid -pthread  -I/usr/include/freetype2 -I/usr/include/libpng16

UTILDIR_OBJS	= $(UTILS_BIN)/colors.o $(UTILS_BIN)/grabclient.o \
		  $(UTILS_BIN)/hsv.o $(UTILS_BIN)/resources.o \
		  $(UTILS_BIN)/usleep.o $(UTILS_BIN)/visual.o \
		  $(UTILS_BIN)/xmu.o $(UTILS_BIN)/xft.o \
		  $(UTILS_BIN)/utf8wc.o $(UTILS_BIN)/logo.o \
		  $(UTILS_BIN)/visual-gl.o \
		  $(UTILS_BIN)/yarandom.o $(UTILS_BIN)/xshm.o \
		  $(UTILS_BIN)/textclient.o $(UTILS_BIN)/async_netdb.o \
		  $(UTILS_BIN)/aligned_malloc.o $(UTILS_BIN)/thread_util.o \
		  $(UTILS_BIN)/spline.o $(UTILS_BIN)/pow2.o \
		  $(UTILS_BIN)/font-retry.o
JWXYZ_OBJS	= $(JWXYZ_BIN)/jwzgles.o
HACKDIR_OBJS	= $(HACK_BIN)/screenhack.o $(HACK_BIN)/xlockmore.o \
		  $(HACK_BIN)/fps.o $(HACK_BIN)/ximage-loader.o \
		  $(HACK_BIN)/ffmpeg-out.o
PNG		= $(HACK_BIN)/ximage-loader.o

HACK_OBJS_1	= fps.o $(UTILS_BIN)/resources.o $(UTILS_BIN)/visual.o \
		  $(UTILS_BIN)/usleep.o $(UTILS_BIN)/yarandom.o \
		  $(UTILS_BIN)/utf8wc.o $(UTILS_BIN)/font-retry.o \
		  $(UTILS_BIN)/xmu.o
HACK_OBJS	= screenhack.o $(HACK_OBJS_1)
XLOCK_OBJS	= screenhack.o xlockmore.o $(COLOR_OBJS) $(HACK_OBJS_1) \
		  $(ERASE)
COLOR_OBJS	= $(UTILS_BIN)/hsv.o $(UTILS_BIN)/colors.o
GRAB_OBJS	= $(UTILS_BIN)/grabclient.o
XSHM_OBJS	= $(UTILS_BIN)/xshm.o $(UTILS_BIN)/aligned_malloc.o


FPS_OBJS	= texfont.o $(HACK_BIN)/fps.o fps-gl.o \

HACK_GLSL_OBJS  = glsl-utils.o
HACK_OBJS	= $(HACK_BIN)/screenhack.o $(HACK_BIN)/xlockmore.o \
		  xlock-gl-utils.o erase-gl.o \
		  $(UTILS_BIN)/resources.o \
		  $(UTILS_BIN)/colors.o \
		  $(UTILS_BIN)/hsv.o \
		  $(UTILS_BIN)/visual.o \
		  $(UTILS_BIN)/visual-gl.o \
		  $(UTILS_BIN)/font-retry.o \
		  $(UTILS_BIN)/pow2.o \
		  $(UTILS_BIN)/utf8wc.o \
		  $(UTILS_BIN)/usleep.o \
		  $(UTILS_BIN)/xmu.o \
		  $(UTILS_BIN)/yarandom.o \
		  ${FPS_OBJS} $(JWZGLES_OBJS) $(HACK_GLSL_OBJS)

# Using $(MAKE) directly means the shell executes things even with "make -n"
MAKE2 = $(MAKE)
MAKE2CC = $(MAKE2) CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

default: all
all: validate_xml $(EXES) $(RETIRED_EXES)

install:   install-program   install-scripts install-xml install-man
uninstall: uninstall-program uninstall-xml uninstall-man

install-strip:
	$(MAKE2) INSTALL_PROGRAM='$(INSTALL_PROGRAM) -s' install

# the hacks, in $HACKDIR
install-program:: $(EXES)
	@if [ ! -d $(install_prefix)$(HACKDIR) ]; then			\
	  $(INSTALL_DIRS) $(install_prefix)$(HACKDIR) ;			\
	 fi ;								\
	for program in $(EXES); do					\
	  echo $(INSTALL_PROGRAM) $$program				\
		$(install_prefix)$(HACKDIR)/$$program ;			\
	  $(INSTALL_PROGRAM) $$program					\
		$(install_prefix)$(HACKDIR)/$$program ;			\
	done

clean::
#	-rm -f *.o a.out core $(EXES) $(RETIRED_EXES) molecules.h
	-rm -f ../../utils/*.o ../*.o *.o a.out core molecules.h

distclean: clean
	-rm -f Makefile TAGS *~ "#"*

# Adds all current dependencies to Makefile
depend:
	$(DEPEND) -s '# DO NOT DELETE: updated by make depend'		    \
	$(DEPEND_FLAGS) --						    \
	$(INCLUDES) $(DEFS) $(DEPEND_DEFINES) $(CFLAGS) $(X_CFLAGS) --	    \
	$(SRCS)

# Adds some dependencies to Makefile.in -- not totally accurate, but pretty
# close.  This excludes dependencies on files in /usr/include, etc.  It tries
# to include only dependencies on files which are themselves a part of this
# package.
distdepend:: molecules.h
	@echo updating dependencies in `pwd`/Makefile.in... ;		    \
	$(DEPEND) -w 0 -f - 						    \
	-s '# DO NOT DELETE: updated by make distdepend' $(DEPEND_FLAGS) -- \
	$(INCLUDES_1) $(DEFS) $(DEPEND_DEFINES) $(CFLAGS) $(X_CFLAGS) --    \
	$(SRCS) 2>/dev/null |						    \
	sort -d |							    \
	(								    \
	  awk '/^# .*Makefile.in ---/,/^# DO .*distdepend/' < Makefile.in ; \
	  sed -e '/^#.*/d'						    \
		  -e 's@ \./@ @g;s@ /[^ ]*@@g;/^.*:$$/d'			    \
		  -e 's@\.\./utils@$$(UTILS_SRC)@g'				    \
		  -e 's@ \([^$$]\)@ $$(srcdir)/\1@g'			    \
		  -e 's@ $$(srcdir)/\(.*config.h\)@ \1@g'			    \
		  -e 's@ $$(srcdir)/\(m6502.h\)@ \1@g'			    \
		  -e 's@ $$(srcdir)/\(images/gen/\)@ \1@g'			    \
		  -e 's@ $$(HACK_SRC)/\(images/gen/\)@ \1@g' ;		    \
	  echo ''							    \
	) > /tmp/distdepend.$$$$ &&					    \
	mv /tmp/distdepend.$$$$ Makefile.in

TAGS: tags
tags:
	find $(srcdir) -name '*.[chly]' -print | xargs etags -a

list_tarfiles:
	@find $(TARFILES) -type f -print | sort

$(UTILS_BIN)/colors.o:		$(UTILS_SRC)/colors.c
$(UTILS_BIN)/grabclient.o:	$(UTILS_SRC)/grabclient.c
$(UTILS_BIN)/hsv.o:		$(UTILS_SRC)/hsv.c
$(UTILS_BIN)/resources.o:	$(UTILS_SRC)/resources.c
$(UTILS_BIN)/usleep.o:		$(UTILS_SRC)/usleep.c
$(UTILS_BIN)/visual.o:		$(UTILS_SRC)/visual.c
$(UTILS_BIN)/xmu.o:		$(UTILS_SRC)/xmu.c
$(UTILS_BIN)/xft.o:		$(UTILS_SRC)/xft.c
$(UTILS_BIN)/utf8wc.o:		$(UTILS_SRC)/utf8wc.c
$(UTILS_BIN)/yarandom.o:	$(UTILS_SRC)/yarandom.c
$(UTILS_BIN)/erase.o:		$(UTILS_SRC)/erase.c
$(UTILS_BIN)/xshm.o:		$(UTILS_SRC)/xshm.c
$(UTILS_BIN)/xdbe.o:		$(UTILS_SRC)/xdbe.c
$(UTILS_BIN)/textclient.o:	$(UTILS_SRC)/textclient.c
$(UTILS_BIN)/aligned_malloc.o:	$(UTILS_SRC)/aligned_malloc.c
$(UTILS_BIN)/thread_util.o:	$(UTILS_SRC)/thread_util.c
$(UTILS_BIN)/pow2.o:		$(UTILS_SRC)/pow2.c
$(UTILS_BIN)/font-retry.o:	$(UTILS_SRC)/font-retry.c
$(HACK_BIN)/screenhack.o:	$(HACK_SRC)/screenhack.c
$(HACK_BIN)/xlockmore.o:	$(HACK_SRC)/xlockmore.c
$(HACK_BIN)/fps.o:		$(HACK_SRC)/fps.c
$(HACK_BIN)/ffmpeg-out.o:	$(HACK_SRC)/ffmpeg-out.c

$(UTILDIR_OBJS):
	$(MAKE2CC) -C $(UTILS_BIN) $(@F)
$(JWXYZ_OBJS):
	$(MAKE2CC) -C $(JWXYZ_BIN) $(@F)
$(HACKDIR_OBJS):
	$(MAKE2CC) -C $(HACK_BIN)  $(@F)

HACK_CFLAGS_BASE=$(INCLUDES) $(DEFS) $(CPPFLAGS) $(CFLAGS) $(X_CFLAGS)
SDL_CFLAGS_BASE=$(SDL3_INCLUDE) $(INCLUDES) $(DEFS) $(CPPFLGAS) $(CFLAGS)

# How we build object files in this directory.
HACK_CFLAGS_BASE=$(INCLUDES) $(DEFS) $(CPPFLAGS) $(CFLAGS) $(X_CFLAGS)
.c.o:
	$(CC) -c $(HACK_CFLAGS_BASE) $<

%.sdl.o: %.c
	$(CC) -c $(SDL_CFLAGS_BASE) -DUSE_SDL -o $@ $<

WINDOWS_CFLAGS = $(WINDOWS_INCLUDE) -DUSE_SDL
%.win.o: %.c
	$(MINGW_PREFIX)gcc -c $(WINDOWS_CFLAGS) -o $@ $<

CC_HACK		= $(CC) $(LDFLAGS)

THREAD_OBJS	     = $(UTILS_BIN)/thread_util.o
TRACK_OBJS	     = rotator.o trackball.o gltrackball.o
HACK_TRACK_OBJS	     = $(HACK_OBJS) $(TRACK_OBJS)
HACK_GRAB_OBJS	     = $(HACK_OBJS) $(GRAB_OBJS)
HACK_TRACK_GRAB_OBJS = $(HACK_TRACK_OBJS) $(GRAB_OBJS)
TEXT		     = $(UTILS_BIN)/textclient.o

hextrail: hextrail.o normals.o $(HACK_TRACK_OBJS)
	$(CC_HACK) -o $@ $^ $(HACK_LIBS) ${X_LIBS}

hextrail-sdl: hextrail.sdl.o normals.sdl.o rotator.sdl.o trackball.sdl.o gltrackball.sdl.o $(HACK_BIN)/screenhack.sdl.o $(HACK_BIN)/xlockmore.sdl.o $(HACK_BIN)/fps.sdl.o $(UTILS_BIN)/yarandom.sdl.o $(UTILS_BIN)/colors.sdl.o $(UTILS_BIN)/hsv.sdl.o
	$(CC_HACK) -o $@ $^ $(HACK_LIBS) $(SDL3_LIBS)

hextrail.exe: hextrail.win.o normals.win.o rotator.win.o trackball.win.o gltrackball.win.o $(HACK_BIN)/screenhack.win.o $(HACK_BIN)/xlockmore.win.o $(HACK_BIN)/fps.win.o $(UTILS_BIN)/yarandom.win.o $(UTILS_BIN)/colors.win.o $(UTILS_BIN)/hsv.win.o
	$(MINGW_PREFIX)gcc -L/usr/x86_64-w64-mingw32/lib -mconsole -o $@ $^ $(WIN_SDL3_LIBS)
