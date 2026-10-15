# Corncob 3-D — Windows port

A faithful C++17/SDL2 port of **Corncob 3-D**, the early-90s DOS flight/combat game, built from its
original MASM source (the `*.ASM`/`*.INC` files in this directory).

## Status

| Phase | Scope | State |
|---|---|---|
| 0 | Foundations: bit-exact ASM math, DGROUP extractor, `GameState`, `TickClock`, tests | ✅ done |
| 1 | 3D engine (`F3DVEC`, `MAT`, `CLIP`, filled polygons) + HD option | next |
| 2 | World (universe/tower files, `MAP.ASM`) | |
| 3 | Player and flight (`3.ASM`) | |
| 4 | Instruments (`INST.ASM`), help screens | |
| 5 | Combat and timed objects (`STRTEXP.ASM`), end-of-mission pictures | |
| 6 | Input and options | |
| 7 | AdLib audio via OPL emulation | |
| 8 | Recreated front end (menus, roster, briefings, high scores) | |
| 9 | Polish and packaging | |

Already playable as a preview: the original intro/story and loading screen, the real instrument panel,
the decoded player plane, and a **placeholder** flight model with a wireframe world (replaced from
Phase 1 on).

## Building (Windows, MSYS2 UCRT64)

```powershell
# one-time: install MSYS2, then in its shell:
#   pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,SDL2} git
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build          # bit-exactness and data tests
```

The build first compiles `tools/asmdata`, which extracts the original data segment from the ASM
sources into `build/generated/DGroup.{cpp,hpp}`.

## Game data

The original shareware data files are **not** in this repository (they're copyrighted). Extract the
distributable game into `assets/original/` so that `assets/original/IMG/3D.IMG` exists. The exe
searches upward from its own folder for `assets/original/`.

## Running

`build\corncob3d.exe`, or the packaged copy in `dist\Corncob3D\`.

| Key | Action |
|---|---|
| Any key / Esc | Next intro scene / skip the intro |
| A / Z, 1–0 | Throttle up / down, presets |
| Arrows | Pitch and roll |
| `,` `.` | Rudder |
| V | Chase view |
| R | Restart after a crash |
| Esc | Quit (in game) |

Command-line options: `--skip-intro`, `--screenshot <file.bmp>`, `--shot-intro <prefix>` (the last two
render unattended, save screenshots, and exit; used for testing).

## Layout

| Path | Contents |
|---|---|
| `*.ASM`, `*.INC`, `*.LNK`, `MAKEFILE` | Original source (the game exe is linked per `3S.LNK`, shareware) |
| `tools/asmdata/` | MASM data-segment extractor (build-time code generator) |
| `src/core/AsmMath.hpp` | Bit-exact equivalents of the original arithmetic macros (`hmul`, `madd`, …) |
| `src/game/GameState.hpp` | The original 64K data segment, accessed by generated label offsets |
| `src/game/TickClock.hpp` | Emulation of the reprogrammed PIT timer |
| `src/assets/` | Loaders for `.VGA`, `.PAL`, `.IMG`, `PICT.DAT`/`TPICT.DAT` |
| `src/ui/Intro.*` | Title, story and loading screens |
| `src/render/` | Current wireframe renderer (to be replaced), framebuffer, 8×8 font |
| `src/platform/` | SDL window and input |
| `tests/` | ctest unit tests |

## File formats decoded so far

- **Object definitions** (`DEFOBJS.ASM`): `dw 0,0,0`, `dw N` parts per view, then 8 view lists (one per
  viewing octant, back-to-front) of `(type, ptr)`. Part types: 8 polygon (`nverts, color, xyz…`),
  6 sphere list (`count`, then `x,y,z,r,color`), 2 frame (wire pointers), 0 null. A color word is a
  literal EGA index or the address of a color byte. Axes: x forward, y span, z up.
- **`.VGA`**: `u16 w-1, u16 h-1`, 8-bit pixels, row padding to a multiple of 8, 2 junk bytes, optional
  256×3 6-bit palette.
- **`.IMG`**: 4-byte header, then `[u16 fill count][u8 byte][u16 literal count][bytes…]` → 640×350 1-bit.
- **`PICT.DAT`**: `nlines, x, y, file, color, (row, col, text)…`, entries separated by `^L`.
  **`TPICT.DAT`**: `x, y, file, nlines, color, (col, row, text)…`, terminated by `-1`.
