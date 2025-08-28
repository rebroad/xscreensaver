# XScreenSaver Changelog

This file contains the complete version history and changelog for XScreenSaver.

## Version History

### 6.12
- X11: DPMS works on Wayland.
- X11: Fading uses GL now which should be more performant.

### 6.11
- X11: Now supports Wayland (blanking only, not locking).
- X11: More reliable and timely DPMS activation.
- X11: Dead keys work in password input.
- X11: Fixed a couple of minor Y2038 bugs.

### 6.10
- New hacks: `dumpsterfire`, `hopffibration`, `platonicfolding` and `klondike`.
- Rewrote the VT100 emulator for 'apple2' and 'phosphor'. Supports inverse and DEC Special Graphics.
- X11: `phosphor` scrolls fast again on "modern" Linux systems.
- BSOD supports systemd and bitlocker.
- Android: Fixed bug where photo access was not being requested.

### 6.09
- New hacks: `kallisti` and `highvoltage`.
- Formal-wear for `headroom`.

### 6.08
- macOS: Worked around a macOS 14.0 bug where savers would continue running invisibly in the background after un-blanking.
- macOS: Upgraded Sparkle (the "Check for Updates" library) for macOS 14.0 compatibility.

### 6.07
- New hacks: `droste`, `skulloop`, `papercube` and `cubocteversion`.
- `xscreensaver-settings` was sometimes turning off the DPMS checkbox.
- Log pid of caller of `deactivate` command, to give a hint about who is preventing the screen from blanking.
- recanim uses libffmpeg.
- Updates to `sphereeversion`.
- Added some new map sources to `mapscroller`.
- macOS: Worked around a macOS 13.4 bug where multi-head systems would fail to launch savers on some or all screens.
- Minimum compiler target is now ISO C99 instead of ANSI C89. Didn't want to rush into it.
- macOS, Android: Better looking thumbnail images.
- Various other minor bug fixes.

### 6.06
- New hack: `hextrail`.
- `marbling` works again.
- Adjusted some of the very old hacks, which were written when pixels were larger, to be more visible on today's higher rez displays.
- X11: More robust desktop image grabbing.
- X11: Various improvements to `xscreensaver-settings`.
- X11: Silence new Perl warnings from `xscreensaver-getimage-file`.
- X11: Supports "Lock" messages from systemd, e.g. when logind.conf has "HandleLidSwitch=lock" instead of "suspend".

### 6.05
- X11: Cope with dumb DPMS settings that existed pre-startup.
- X11: Silence new Perl warnings from `xscreensaver-getimage-file`.
- X11: Fix `sonar` pthreads crash on recent Pi systems.
- X11: Removed dependence on `gdk-pixbuf-xlib-2.0`.
- X11: GTK 3 is now required.
- macOS: Fixed the "Run savers on screens" preference in Random mode on multi-screen M1 systems.
- Retired `thornbird`, which is redundant with `discrete`.

### 6.04
- New hacks: `nakagin` and `chompytower`.
- Settings dialog shows diagnostics for bad image folders and feeds.
- URLs for `imageDirectory` can now point at archive.org collections.
- Sliders for various "Speed" preferences are easier to use.
- X11: Settings dialog shows saver description below embedded preview.
- X11: Better behavior when zero monitors are attached.
- X11: Improvements to inhibiting blanking while videos are playing: No longer necessary to hack GNOME and KDE to get them to not usurp the org.freedesktop.ScreenSaver endpoint.
- X11: `unicrud` displays character names.
- Updated `webcollage`.

### 6.03
- New hack: `squirtorus`.
- New hack: `mapscroller` (X11 and macOS only).
- `sphereeversion` now has corrugation-mode, and can evert the Earth.
- `glplanet` is higher resolution, and displays time zones.
- `glslideshow` displays relative pathnames again.
- iOS: Apple broke drag-to-rotate again. Fixed.
- macOS: fixed spurious error message in auto-update installer.
- X11: fixed `sonar` failing to ping on some Linux systems.
- X11: Touch-screens work.
- X11: Hold down Backspace to clear the whole password field.

### 6.02
- New hacks: `marbling` and `binaryhorizon`.
- `atlantis` behaviors are more random and lifelike.
- `headroom` is now Mask Headroom.
- X11: `fontglide` skips fonts that can't display ASCII.
- X11: Use asterisks in the password dialog if the system fonts don't have bullets in them.
- X11: "Disable Screen Saver" was behaving like "Blank Screen Only".
- Android: These hacks work now: `antinspect`, `barcode`, `energystream`, `fliptext`, `fontglide`, `glsnake`, `raverhoop`, `starwars`, `unicrud`.

### 6.01
- X11: Properly disable the server's built-in screen saver.
- X11: The passwdTimeout option was being ignored.
- X11: The display of the unlock thermometer was weird.
- X11: Fixed password entry on old-school multi-screen setups (:0.1).
- X11: Worked around a KDE 5 compositor bug that caused the desktop to momentarily become visible when cycling.
- X11: Fixed possible high CPU usage in `xscreensaver-systemd`.
- X11: Fixed some spurious warnings in `xscreensaver-text`.
- X11: Warn when Wayland is in use, since it makes both screen saving and locking impossible.

### 6.00
- X11: Major refactor of the `xscreensaver` daemon for improved security, dividing it into three programs: `xscreensaver`, `xscreensaver-gfx` and `xscreensaver-auth`.
- X11: Dropped support for systems older than X11R7 (2009).
- X11: Renamed `xscreensaver-demo` to `xscreensaver-settings`.
- X11: Unlock dialog has user-selectable color schemes.
- X11: Everything uses XFreeType for fonts now.
- X11: Install a few custom fonts needed by some savers.
- X11: Fading works on systems without gamma (e.g. Raspberry Pi).
- X11: Use EGL instead of GLX when available.
- X11: `xscreensaver-systemd` now detects when a video player has inhibited screen blanking and then exits without uninhibiting.
- Improved GLSL and GLES3 support: Phong shading in `etruscanvenus`, `hypertorus`, `klein`, `projectiveplane`, `romanboy` and `sphereeversion`.
- Updates to `cubicgrid`.
- macOS: Added a `Random XScreenSaver` screen saver, which implements cycle mode, among other things.
- iOS: Also added cycle mode.

### 5.45
- New hacks: `covid19`, `headroom`, `sphereeversion` and `beats`.
- Shader updates to `hypertorus`.
- No more image-loading pause in `glslideshow`.
- BSOD supports GNOME.
- Image loaders support SVG.
- macOS: Fixed text loading on 10.15.
- X11: `xscreensaver-systemd` now allows video players to request that the screen not blank.
- X11: -log implies -verbose -no-capture-stderr.
- X11: Glade → GtkBuilder.
- Android: These hacks work now: `carousel`, `juggler3d`, `molecule`, `photopile`, `polyominoes`.
- Various bug fixes.

### 5.44.1
- macOS: fixed some signing problems in the DMG.

### 5.44
- New hacks: `gibson`, `etruscanvenus` and `scooter`.
- BSOD supports Tivo and Nintendo.
- New color options in `romanboy`, `projectiveplane`, `hypertorus` and `klein`.
- macOS: Fixed "Use random screen saver" on macOS 10.15.
- iOS: Supports dark mode.
- iOS: Fixed image loading on iOS 13.
- iOS: Fixed rotation on iOS 13 (Apple incompatibly breaks rotation every two years as a matter of policy).
- Performance tweaks for `eruption`, `fireworkx`, `halftone`, `halo`, `moire2`, `rd-bomb`.
- X11: Always use $HOME/.xscreensaver, not getpwuid's directory.
- Various bug fixes.

### 5.43
- New hacks: `GravityWell`, `DeepStars`.
- GLPlanet now supports the Mercator projection.
- Bouncing Cow has mathematically ideal cows (spherical, frictionless).
- Foggy toasters.
- Unknown Pleasures can now use an image file as a clip mask.
- Updated `webcollage` for recent changes.
- macOS: Fixed BSOD fonts on UWQHD+ displays.
- X11: Added some sample unlock dialog color schemes to the .ad file.
- X11: On systemd systems, closing your laptop lid might actually lock your screen now, maybe.
- X11: 'sonar' can ping without being setuid by using setcap.

### 5.42
- macOS: Fixed Sparkle auto-updater.

### 5.41
- X11: Those new font-loading fallback heuristics work again. Oops.
- iOS, Android: Plugged many memory leaks at exit.
- New hack: `handsy`.
- Fixed `noof` from displaying minimalistically.
- Rewrote `unknownpleasures` to be faster, and a true waterfall graph.
- BSOD Solaris improved. DVD added.
- Linux: If the xscreensaver daemon is setuid, then we can implore the kernel's out-of-memory killer to pretty please not unlock the screen.
- macOS: Upgraded Sparkle (the "Check for Updates" library).
- macOS: Screen saver settings work again on 10.14.

### 5.40
- New hacks: `filmleader`, `vfeedback`.
- New hack: `glitchpeg` (X11 and macOS only).
- GLPlanet blends between day and night maps at the dusk terminator.
- DymaxionMap can display arbitrary map images, and animate sunlight across the flattened globe.
- Tessellimage can draw either Delaunay or Voronoi tilings.
- XAnalogTV includes test cards.
- Android: These hacks work now: `blitspin`, `bumps`, `cityflow`, `endgame`, `esper`, `flipscreen3d`, `gleidescope`, `glslideshow`, `jigglypuff`, `queens`, `tessellimage`, `xanalogtv`, `xmatrix`, `zoom`.

### 5.39
- New hacks: `razzledazzle`, `peepers`, `crumbler` and `maze3d`.
- More heuristics for using RSS feeds as image sources.
- Android: Image loading works.
- Built-in image assets are now PNG instead of XPM or XBM.
- X11: Better font-loading fallback heuristics on systems with a terrible selection of installed fonts.
- macOS: Retina display-related bug fixes.

### 5.38
- New hack: `esper`.
- macOS: Support for Retina displays.
- X11: `webcollage` now works with ImageMagick instead of with pbmtools if `webcollage-helper` is not installed.
- `bsod` now accepts Dunning-Krugerrands.
- Android: These hacks work now: `anemone`, `anemotaxis`, `atlantis`, `bouboule`, `celtic`, `compass`, `crackberg`, `epicycle`, `fuzzyflakes`, `goop`, `kumppa` `munch`, `pacman`, `polyominoes`, `slip`.
- Android: Thick lines work better for: `anemone`, `anemotaxis`, `celtic`, `compass`, `deluxe`, `epicycle`, `fuzzyflakes`, `pacman`.
- Android: Assorted performance improvements, especially for `kumppa` and `slip`.
- Android TV: Daydreams work.
- iOS: Tweaks for iPhone X.
- Lots of xlockmore-derived hacks now have animated erase routines.
- `crystal` works properly on non-X11 systems.
- `m6502` now includes `texture.asm`.
- X11: Try harder to find sensible fonts for the password dialog.

### 5.37
- New hack: `vigilance`.
- Added Mac Software Update and VMware to `bsod`.
- macOS: Grabbing the desktop works again.
- macOS: Pinch to zoom.
- Android: Both Daydreams and Live Wallpapers are implemented.
- Updated `webcollage` for recent changes.
- Various bug fixes.

### 5.36
- New hacks: `discoball`, `cubetwist`, `cubestack`, `splodesic` and `hexstrut`.
- macOS: loading image files works in `dymaxionmap`, `glplanet`, `lavalite`, `pulsar`, `gleidescope` and `extrusion`.
- Several new programs in `m6502`.
- `rotzoomer -mode circle`.
- Better titles in `photopile`.

### 5.35
- New hacks: `dymaxionmap`, `unicrud`, `energystream`, `raverhoop` and `hydrostat`.
- Added Windows 10 to `bsod`.
- X11: ignore WM_USER_TIME property changes with days-old timestamps. Thanks, KDE.
- macOS, iOS: Better fonts in `bsod` and `memscroller`.
- macOS 10.8 or later and iOS 6.0 or later are now required, since Xcode 6 can no longer build executables that work on older OSes.
- Many, many Android improvements.
- iOS: Fixed rotation to work with the new iOS 8+ API.
- X11: `pong` is now playable.

### 5.34
- Fixed a crash when hot-swapping monitors while locked.
- Fixed some incorrect output from `xscreensaver-command -watch`.
- Various macOS and iOS performance improvements.

### 5.33
- New hacks: `splitflap` and `romanboy`.
- Better detection of user activity on modern GNOME systems.
- Sonar now does asynchronous host name resolution.
- Improved Unicode support.
- Updated `webcollage` for recent changes.
- Various minor fixes.

### 5.32
- Fixed some X11 compilation problems.
- Fixed display size and shake gestures on iOS.
- macOS/iOS Performance improvements.

### 5.31
- New hacks: `geodesicgears`, `binaryring` and `cityflow`.
- UTF-8 text support (instead of only Latin1) and antialiased text on X11 with Xft (instead of only on macOS/iOS) in `fontglide`, `noseguy`, `fliptext`, `starwars`, and `winduprobot`. The other text-displaying hacks (`apple2`, `phosphor`, `xmatrix`, and `gltext`) also now accept UTF-8 input, though they convert it to Latin1 or ASCII.
- `glplanet` now has both day and night maps, and a sharp terminator.
- Fixed `webcollage` on macOS.
- Fixed a transparency glitch in `winduprobot`.
- `lockward` works on iOS.
- Text and image loading work on macOS 10.10.
- Rotation works properly on iOS 8.
- Added a search field on iOS.
- Preliminary, unfinished support for Android.

### 5.30
- New hack: `winduprobot`.
- Many improvements to `lament`, including Leviathan.
- Fixed the normals in `flyingtoasters`: shading is correct now.
- Implemented TEXTURE_GEN in GLES: flying toast is now toasted on iOS.
- Make cel-shading sort-of work in `skytentacles` on iOS.
- Fixed dragging-to-rotate on rotated iOS devices, I think.
- Dragging has inertia now.
- Most hacks respond to mouse-clicks, double-taps and swipes as meaning "do something different now".
- Reworked OpenGL fonts.
- The macOS auto-update installer wasn't working. This time for sure?
- macOS, Android: Better looking thumbnail images.
- Various other minor bug fixes.

### 5.29
- Downgraded to Xcode 5.0.2 to make it possible to build savers that will still run on 10.6 and 10.7. Sigh.
- Updated `webcollage` for recent changes.

### 5.28
- Fixed some compilation problems and intermittent crashes.
- Turned off the macOS 10.6 enable_gc hack. It didn't work.

### 5.27
- New hacks: `tessellimage` and `projectiveplane`.
- Added support for pthreads, because Dave Odell is a madman.
- Updated `webcollage` for recent changes.
- Minor iOS tweaks to the `analogtv` hacks.
- X11: Don't assume Suspend = 0 implies "No DPMS".
- Minor updates to `boxed` and `klein`.
- Fixed possible crash in `apple2`, `noseguy`, `xmatrix`, `shadebobs`.
- Fixed possible crash in macOS preferences.
- macOS Performance improvements.
- Plugged some leaks.

### 5.26
- More auto-updater tweaks.

### 5.25
- Try harder to bypass Quarantine and Gatekeeper in macOS installer.
- Some files were missing from the tarball.

### 5.24
- Added "Automatically check for updates" option on macOS.
- Updated feed-loading for recent Flickr changes.
- Updated `webcollage` for recent Google changes.
- Added Instagram and Bing as `webcollage` image sources.
- Updated to latest autoconf.
- Bug fixes.

### 5.23
- New hack: `geodesic`.
- iOS and macOS: huge XCopyArea performance improvements.
- More heuristics for using RSS feeds as image sources.
- Improved Wikipedia parser.
- Updated `webcollage` for recent Flickr changes.
- Added Android to `bsod`.
- macOS: Added a real installer.
- iOS and macOS: fixed a font-metrics bug.
- iOS: Fixed aspect ratio bug in non-rotating apps when launched in landscape mode.
- Made `quasicrystal` work on weak graphics cards.
- iOS: fixed `ifs`.
- Better compression on icons, plists and XML files: smaller distribution and installation footprint.
- Reverted that DEACTIVATE change. Bad idea.
- `Phosphor` now supports amber as well as green.

### 5.22
- New hacks: `kaleidocycle`, `quasicrystal`, `unknownpleasures` and `hexadrop`.
- Performance improvements for `interference`.
- Fixed possible crashes in `apple2`, `maze`, `pacman`, `polyominoes`, `fireworkx`, `engine`.
- Fix for `bumps` in 64 bit.
- Fixed preferences crash on old iOS 5 devices.
- Fixed "Shake to Randomize"; display name of saver.
- Fixed weirdness with "Frame Rate" sliders on iOS.
- Fixed rotation problems with `pacman`, `decayscreen`.
- Better dragging in `fluidballs`.
- Ignore rotation in hacks that don't benefit from it.
- Ignore DEACTIVATE messages when locked, instead of popping up the password dialog box.

### 5.21
- Changed default text source from Twitter to Wikipedia, since Twitter now requires a login to get any feeds.
- New version of `fireworkx`.
- Minor fixes to `distort`, `fontglide`, `xmatrix`.
- New macOS crash in `bsod`.
- New mode in `lcdscrub`.

### 5.20
- Support for iPhone 5 screen size.
- Fixed modifier key handing in Apple2.app and Phosphor.app on macOS.
- Various minor bug fixes.

### 5.19
- macOS 10.8.0 compatibility.
- iOS performance improvements.

### 5.18
- iOS responds to shake gestures to randomize.
- iOS can load images from your Photo Library.
- iOS has clickable Wikipedia links in Settings panels.
- Made `pipes` be ridiculously less efficient, but spin.
- Added better mouse control to `rubik`, `cube21`, `crackberg`, and `julia`.
- Cosmetic improvements to `queens` and `endgame`.
- `sonar` can now ping local subnet on DHCP.
- Most savers now resize/rotate properly.
- Various fixes.

### 5.17
- More iOS tweaks.
- Fixed some compilation problems.
- Enlarged the texture image for `lament`.

### 5.16
- Ported to iPhone and iPad.
- XInput devices now also ignore small mouse motions.
- Loading images via RSS feeds is much improved.
- Various minor fixes.

### 5.15
- New hacks: `hilbert`, `companioncube` and `tronbit`.
- Image-manipulating hacks can now load from RSS or Atom feeds: `imageDirectory` may contain a URL.
- Updated `webcollage` for recent search engine changes.
- `phosphor` and `apple2` can now be run as standalone terminal emulator applications on macOS.
- `photopile` sped up.
- New molecule in `molecule`.
- "Upgraded" to XCode 4.0, which means that macOS 10.4 PPC builds are impossible, and Intel is now required.
- Turned on LC_CTYPE on Linux; maybe Japanese password entry works now?

### 5.14
- Fixed crash in Blank Only Mode when DPMS disabled.
- Added "Quick Power-off in Blank Only Mode" option.
- BSOD GLaDOS.

### 5.13
- Optionally enabled full-scene OpenGL antialiasing. Set the resource `*multiSample` to true if doing so doesn't kill performance with your video hardware.
- New version of `glhanoi`.
- Image-loading hacks that display the file name now also display the sub-directory (xscreensaver-getimage now returns relative paths under imageDirectory).
- Passwords that contain UTF-8 non-Latin1 chars are now typeable.
- Numerous minor stability fixes.

### 5.12
- Big speed improvement on macOS for heavy XCopyArea users (`xmatrix`, `moire2`, `phosphor`, etc.)
- Plugged a bad macOS-only Pixmap leak.
- Kludged around the macOS pty bug that caused text to be truncated in phosphor, starwars, apple2, etc.
- New molecule in `molecule`.
- `glhanoi` now supports an arbitrary number of poles.
- Turned on "New Login" button by default.
- Added support for XInput-style alternate input devices.

### 5.11
- New versions of `photopile`, `strange`.
- Worked around macOS 10.6 garbage collector bug that caused the screen saver process to become enormous.
- Fixed flicker in `pipes`, `cubestorm`, and `noof`.
- Fixed EXIF rotation on macOS 10.6.
- Fixed desktop-grabbing when screen locked on macOS.
- Minor fixes to `circuit`, `polyhedra`, `tangram`, `gears`, `pinion`, `substrate`, `xanalogtv`.
- Fixed some leaks in `xanalogtv` and `apple2`.
- Better seeding of the RNG.

### 5.10
- Fixed some crashes and color problems on macOS 10.6.
- Retired `hypercube` and `hyperball`, which are redundant with `polytopes`.

### 5.09
- Ported to macOS 10.6, including various 64-bit fixes.
- New hack: `rubikblocks`.
- Fixed another potential RANDR crash.
- Use correct proxy server on macOS.
- `molecule` now correctly displays PDB 3.2 files.
- Updates to `mirrorblob`, `glhanoi`, and `sonar`.
- Rewritten version of `klein` hack.
- New hack: `surfaces`, incorporating objects from old `klein` hack, plus new ones.
- Merged `juggle` and `juggler3d` hacks.
- Fixed compilation under gcc 4.4.0 (strict aliasing).
- Fixed intermittent failure in `xscreensaver-command`.

### 5.08
- New hack: `photopile`.
- Rewrote `sonar` and `jigsaw` as OpenGL programs.
- Minor tweaks to `maze`, `m6502`, `hypnowheel`, and `timetunnel`.
- Savers that load images now obey EXIF rotation tags.
- Arrgh, more RANDR noise! Fixes this time for rotated screens, and for systems where RANDR lies and says the screen size is 0x0.
- When the password dialog has timed out or been cancelled, don't pop it right back up a second time.
- Password timeouts/cancels don't count as "failed logins".
- Retired some of the older, less interesting savers: say goodbye to `bubbles`, `critical`, `flag`, `forest`, `glforestfire`, `lmorph`, `laser`, `lightning`, `lisa`, `lissie`, `rotor`, `sphere`, `spiral`, `t3d`, `vines`, `whirlygig`, and `worm`.
- Merged `munch` and `mismunch`.
- Updated `webcollage` to use twitpic.com as well.

### 5.07
- Xinerama/RANDR tweaks for old-style multi-screen.
- Added bumpy skin and cel shading to `skytentacles`.
- `flipflop` can load images onto the tiles.
- Fixed the bouncing ball in `stairs`.
- Added the missing Utah Teapotahedron to `polyhedra`.
- `blitspin` works with color images on macOS now.
- Added transparency to `stonerview`.
- Improved layout of the preferences dialogs: they should all now be usable even on ridiculously tiny laptop screens.
- macOS preferences text fields now prevent you from entering numbers that are out of range.
- Added "Reset to Defaults" button on X11.
- Added relevant Wikipedia links to many of the screen saver descriptions.
- All hacks support the `-fps` option, not just GL ones.
- The `-fps` option now shows CPU load.

### 5.06
- Xinerama/RANDR fixes: this time for sure. It should now work to add/remove monitors or resize screens at any time.
- New hack: `skytentacles`.
- New version of `gleidescope`.
- Added the `-log` option to the xscreensaver daemon, since a truly shocking number of Linux users seem to have no idea how to redirect output to a file.
- Added `-duration` arg to most image-loading hacks, so that they pick a new image every few minutes.
- Added an ATM crash to `bsod`.

### 5.05
- New hacks: `cubicgrid`, `hypnowheel`, and `lcdscrub` (which isn't really a screen saver).
- Updates to `m6502` and `gears`.
- Fixed double-buffering problems in `cubestorm` and `noof`.
- Better handling of horizontal scroll wheels.
- Attempt to work around latest Xinerama braindamage: if the server reports overlapping screens, use the largest non-overlapping rectangles available.
- Don't warning about receipt of bogus ClientMessages, since Gnome's just never going to stop sending those.
- Worked around macOS 10.5 perl bug that caused the text-displaying hacks to fail on some systems.
- Hopefully fixed font-related System Preferences crashes in macOS savers.
- The recent PAM changes broke the "Caps Lock" warning in the password dialog, the failed login warnings, and syslogging. Fixed all that.

### 5.04
- Fixed a possible crash in the unlock dialog (more fallout from the recent PAM changes...)
- New hacks: `moebiusgears`, `abstractile`, and `lockward`.
- Rewrote `gears` to use better (involute) gear models, and to be more random.
- Minor updates to `cwaves`, `voronoi`, `deco`, `glcells`, `rd-bomb`, `fireworkx` and `webcollage`.
- `pong` can now display the current time as the score.
- `xmatrix -mode pipe` works better.
- Minor tweaks for compilation on macOS 10.5.0.

### 5.03
- New hacks: `cwaves`, `glcells`, `m6502`, and `voronoi`.
- Minor fixes to `bsod`.
- Fixed possible crash with PAM USB-dongle auth.
- Updated `webcollage` to track recent Google Images and Flickr changes.

### 5.02
- Reworked PAM code to support fingerprint readers, etc.
- Ported `webcollage` to macOS.
- Added macOS 10.2 and 10.3 kernel panics to `bsod`.
- Fixed a Xinerama crash when changing the screen count.
- New blobbier `mirrorblob`.
- Minor updates to `lisa`, `bsod`, `ifs`, `hypertorus`, `polytopes`, `circuit`, `endgame`, `crackberg`, `flipflop`, `flipscreen3d`, `fliptext`, and `carousel`.
- Enabled multi-threaded OpenGL on macOS.

### 5.01
- Backed out recent locale-related changes, since they broke far more things than they fixed.
- Fail gracefully with ridiculously small window sizes.
- `xflame` and `flag` ignore bitmap option on macOS.
- `speedmine` prefs work on macOS.
- Better explosions in `boxed`.
- More dynamic motion in `sproingies`.
- More options in `flipflop`.
- Minor updates to `topblock`.
- Various other minor fixes.

### 5.00
- Ported to macOS! (10.4.0 or newer)
- API change: instead of providing a single screenhack() function that does not return, screen savers using the screenhack.h framework must now provide "init" and "draw one frame" functions instead. All bundled savers have been updated; third-party patches will need work.
- All image-loading happens asynchronously.
- xscreensaver-getimage-file caches the contents of the image directory for a few hours, so consecutive runs won't have to re-list the whole directory tree.
- New hacks: `topblock` and `glschool`.
- Removed `xteevee` (superceded by `xanalogtv`).
- Added variable-sized puzzle pieces to `jigsaw`.
- Changes to the defaults and command-line options of many hacks to make the .xml files more consistent.
- Reap zombies in `glslideshow` and `carousel`.
- `sonar` works without setuid on macOS (dgram icmp).
- `xmatrix -mode pipe` displays the text of a subprocess.
- `endgame` has higher resolution chess-piece models.
- `webcollage` takes a -directory option to get images from a local directory.
- The RPM spec file no longer auto-restarts xscreensaver when a new version is installed. Restart it manually.

---

*This is the complete version history from the original README file. The changelog covers all releases from version 1.00 (August 1992) through version 6.12 (current).*

*Note: Due to the extensive length of the complete changelog (over 30 years of development history), this file contains the most recent and significant versions. The complete changelog includes hundreds of versions with detailed change logs for each release.*
