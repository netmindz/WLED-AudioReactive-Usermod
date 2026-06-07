# AGENTS.md — Build Verification Guide

This document explains how an automated agent (or a developer) can compile-test
the OO library against a real WLED firmware build to verify that it integrates
correctly as a drop-in replacement for the monolithic `audioreactive` usermod.

---

## Prerequisites

| Tool | Notes |
|---|---|
| [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) | `pip install platformio` or the VSCode extension |
| WLED checkout | See "Choosing a WLED fork" below |

### Choosing a WLED fork

Two forks can be used; choose based on what you need to test:

| Fork | Defines | Use when |
|---|---|---|
| [`wled/WLED`](https://github.com/wled/WLED) (`main`) | — | Basic compile check; no MoonModules-specific paths |
| [`MoonModules/WLED-MM`](https://github.com/MoonModules/WLED-MM) (`mdev`) | `_MoonModules_WLED_`, `WLEDMM_FASTPATH`, etc. | Full parity with the original MoonModules source |

The reference OO code was ported from `MoonModules/WLED-MM main`, so
**full parity verification requires the `WLED-MM` fork**.

Clone whichever fork alongside this repo (sibling directory is conventional):

```sh
git clone --depth=1 https://github.com/MoonModules/WLED-MM.git ../WLED-MM
# or
git clone --depth=1 https://github.com/wled/WLED.git            ../WLED
```

---

## How WLED loads usermods

WLED's `pio-scripts/load_usermods.py` processes the `custom_usermods` key from
`platformio.ini`. A bare name like `audioreactive` is resolved to
`usermods/<name>/` inside the WLED tree. An external path (e.g.
`symlink:///abs/path`) is passed directly to `lib_deps`.

A library is recognised as a *WLED module* (and gets WLED's `src/` prepended to
its include path, which is required for `wled.h` to resolve) when any of these
is true:

- the library source lives under `WLED/usermods/`
- `library.json` `"name"` starts with `"wled-"` ← **our library qualifies**
- the dep name was listed explicitly in `custom_usermods` as an external entry

Because our `library.json` name is `wled-audioreactive`, the script recognises
it automatically without any special configuration.

---

## Method A — `platformio_override.ini` (recommended, non-destructive)

WLED reads `platformio_override.ini` if it exists and merges it with
`platformio.ini`. This file is already in WLED's `.gitignore`, so it is safe to
create locally without affecting the WLED tree.

Create `<WLED-checkout>/platformio_override.ini` with the following content
(replace the path with the absolute path to this repo):

```ini
[env:esp32dev_oo]
extends = env:esp32dev
custom_usermods = symlink:///absolute/path/to/WLED-AudioReactive-Usermod
```

> **Why override `custom_usermods`?**
> PlatformIO `extends` semantics: a child env's `custom_usermods` **replaces**
> the parent's value entirely. Setting it to the OO symlink path means only
> this library is loaded — the monolithic `usermods/audioreactive/` is not
> linked, avoiding a name/symbol collision.

Build the new env:

```sh
cd /path/to/WLED-checkout
pio run -e esp32dev_oo
```

A successful build (zero errors) confirms the OO library is a valid drop-in.

---

## Method B — Symlink replacement (modifies the WLED tree)

Replace the monolithic usermod folder with a symlink to this repo:

```sh
cd /path/to/WLED-checkout
rm -rf usermods/audioreactive
ln -s /absolute/path/to/WLED-AudioReactive-Usermod usermods/audioreactive
```

Then build any env that uses `custom_usermods = audioreactive`:

```sh
pio run -e esp32dev
```

Restore afterwards:

```sh
rm usermods/audioreactive
git checkout -- usermods/audioreactive
```

---

## Known pre-existing issues to ignore

The `examples/` directory in *this* repo contains a standalone `platformio.ini`
for the OO library in isolation. It has two **pre-existing** build errors that
exist on the unmodified branch and are unrelated to the OO refactor:

1. `arduinoFFT` fork mismatch — `examples/platformio.ini` pins
   `kosme/arduinoFFT@2.0.1`; the library and WLED expect
   `softhack007/arduinoFFT@1.9.2`. The API differs (`MajorPeak` signature).
2. `audio_source.h` incomplete types — this header depends on WLED internals
   (`WS2812FX`, `Segment`, etc.) that are not present in the examples build.

These errors appear when running `pio run` inside `examples/`. They do **not**
appear in a full WLED build and should be ignored during integration testing.

---

## Quick-reference commands

```sh
# Full WLED-MM build with the OO library (after creating platformio_override.ini)
cd ../WLED-MM
pio run -e esp32dev_oo 2>&1 | tail -30

# Check only for compilation errors (no link/upload)
pio run -e esp32dev_oo --target compiledb

# Diff the OO library against the original MoonModules main (for parity checks)
diff /tmp/main_audio_reactive.h \
     /path/to/WLED-AudioReactive-Usermod/audio_reactive.h | less
```
