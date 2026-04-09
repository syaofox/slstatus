# slstatus Agent Guide

## Build Commands
- `make clean install` - Build and install (default prefix: `/usr/local`)
- `make clean` - Remove build artifacts
- `make dist` - Create distribution tarball

## Configuration
- Edit `config.mk` to customize paths (PREFIX, MANPREFIX), X11 paths, and compiler flags
- Create `config.h` by copying `config.def.h` and editing the `args[]` array to select which components to build
- The Makefile auto-generates `config.h` from `config.def.h` if it doesn't exist

## Component Selection
Components are compiled in based on what's defined in `config.h` `args[]` array. Comment out entries to disable components.

## Platform Notes
- Linux: Requires Xlib (`-lX11`)
- FreeBSD: Add `-lkvm -lsndio` to LDLIBS
- OpenBSD: Add `-lsndio` to LDLIBS

## Key Source Files
- `slstatus.c` - main loop
- `components/*.c` - individual status components
- `util.c` / `util.h` - utility functions
- `arg.h` - argument structure definitions

## No Test Suite
This project has no automated tests. Manual verification is required.