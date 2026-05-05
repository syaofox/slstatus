# slstatus Agent Guide

## Build
- `make` — build the `slstatus` binary
- `make clean install` — build and install (PREFIX from `config.mk`, default `/usr/local`)
- `make clean` — remove build artifacts
- `make dist` — create distribution tarball

## Configuration
- `config.h` is auto-generated from `config.def.h` if missing (Makefile rule). Edit `config.h` after copying to customize which components run. `config.h` is in `.gitignore`.
- `config.mk` controls install paths, X11 paths, compiler flags (POSIX make, `.POSIX:` set).
- Recompilation is required for any config change.

## Runtime
- `slstatus` — outputs to `WM_NAME` via X11 (for dwm). `slstatus -s` — print to stdout. `slstatus -1` — single shot to stdout then exit. `slstatus -v` — print version.
- SIGUSR1 triggers an instant redraw (does not exit). SIGINT/SIGTERM exit cleanly.
- When a component function returns NULL, `unknown_str` ("n/a") is shown instead.
- Color support via Xresources: set `res_name` in `args[]`, define `slstatus.<name>.color: #RRGGBB` in Xresources.

## Key files
- `slstatus.c` — main loop, arg struct, X11/Xresources integration
- `slstatus.h` — all component function declarations
- `arg.h` — suckless ARGBEGIN/ARGEND argument parsing macros
- `util.c` / `util.h` — shared helpers (`bprintf`, `pscanf`, `lscanf`, `fmt_human`, `esnprintf`)
- `components/*.c` — individual status functions (OS-specific via `#if defined(__linux__)` / `__OpenBSD__` / `__FreeBSD__`)

## No tests
Manual verification only.
