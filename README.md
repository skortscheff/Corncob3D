# Corncob 3-D — Windows port

A faithful C++17/SDL2 port of **Corncob 3-D**, the early-90s DOS flight/combat game, built from its
original MASM source (the `*.ASM`/`*.INC` files in this directory).

## Status

| Phase | Scope | State |
|---|---|---|
| 0 | Foundations: bit-exact ASM math, DGROUP extractor, `GameState`, `TickClock`, tests | ✅ done |
| 1 | 3D engine (`F3DVEC`, `MAT`, `HLINE`, filled polygons) + HD option | ✅ done |
| 2 | World (universe/tower files, `MAP.ASM`) | next |
| 3 | Player and flight (`3.ASM`) | |
| 4 | Instruments (`INST.ASM`), help screens | |
| 5 | Combat and timed objects (`STRTEXP.ASM`), end-of-mission pictures | |
| 6 | Input and options | |
| 7 | AdLib audio via OPL emulation | |
| 8 | Recreated front end (menus, roster, briefings, high scores) | |
| 9 | Polish and packaging | |

Playable as a preview: the original intro/story and loading screen, the real instrument panel, and
every frame drawn by the ported 3D engine (filled sky/ground horizon, polygons, spheres, wire frames,
rear-view window, page flipping, palette). The observer is steered by a **placeholder** controller,
and a few real object templates stand in for the world until Phases 2–3.
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
| A / Z | Speed up / down (placeholder controller) |
| Arrows | Pitch and roll |
| `,` `.` | Yaw |
| R | Level the wings |
| T | Rear-view window on/off |
| F | Horizon fill on/off (the original's wireframe-horizon mode) |
| H | HD mode (2× resolution) on/off |
| Esc | Quit (in game) |
Command-line options: `--skip-intro`, `--hd <n>`, `--screenshot <file.bmp>` (optionally with
`--pose z,yaw,pitch,roll[,fwd]`), `--shot-intro <prefix>` (the screenshot options render unattended,
save BMPs, and exit; used for testing).

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
| `src/engine/` | The 3D engine: `F3dVec` (F3DVEC/MAT/HLINE/KLINE), `AsmTables` (SIN/CSQRT tables) |
| `src/render/` | `EgaScreen` (two indexed EGA pages, palette, presentation), framebuffer, 8×8 font |
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

## 3D engine notes

The engine works on the original data segment: object shapes, matrices and scratch buffers are the
DGROUP bytes, addressed by the same offsets the ASM uses. Known differences from the original:

- **`sas.lib` routines** (the author's math library) aren't in the source: `divrs_dw` is implemented
  as a rounded signed divide (matching the library's `.divr_*` macros), `div_dw`/`muls_*` from their
  macro definitions, and `rand_w` is a stand-in LCG.
- **Lines**: KLINE's run-slice routine for long shallow lines is drawn with EGALINE's Bresenham; the
  same pixels give or take one per row.
- **Dithered fills** (colors above 15): the pattern is applied by absolute pixel position; the ASM
  shifts it by the start pixel in the first byte of long spans.
- **Divide overflow**: the 8086 would trap (and DOS abort); the port saturates the quotient.
- `CLIP.ASM` is only linked in the Hercules build; the EGA build clips with F3DVEC's own `oclip`.
- Not yet ported: ground dots (`g3dobj`, Phase 3), `getang` (needs `ATN.ASM`), tile/memseg tables
  (Phase 2), and the collision/timed-object calls inside `drawnear` (stubbed until Phases 3 and 5).