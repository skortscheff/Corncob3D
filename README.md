# Corncob 3-D — Windows port

A C++17 / SDL2 port of **Corncob 3-D**, the early-90s DOS flight and combat game by Kevin Stokes, built from its
original MASM source (the `*.ASM` / `*.INC` files in this directory). The flight model, the 3D engine, the enemies and the
mission logic are translated routine by routine from the assembly, keeping its fixed-point arithmetic and quirks. The menu
programs of the original (MOAG, TOWER, ISCORE) have no source, so the front end is rebuilt from their text and the manual.

## Status

Everything in the plan is in and plays: title sequence and music, menus, pilots and careers, training and combat missions in
the four shipped theaters, AdLib sound, the control tower, the end-of-mission pictures and the lost-in-space sequence.
`docs/PORT_PLAN.md` has the phase by phase record, the deliberate deviations and what was left out.

Not done: several airfields per theater (the original supports nine, the shipped theaters have one), printing the
documentation, real DOS joystick calibration. The raw-score weights, rank thresholds and medal rules are recreations; only the
three score examples of the manual are original.

## Building (Windows, MSYS2 UCRT64)

```powershell
# one-time: install MSYS2, then in its shell:
#   pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,SDL2} git
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build          # unit tests, flight tests over the theaters, the menus driven end to end
cmake --build build --target package    # build/dist/Corncob3D: the exe, SDL2.dll and this README
```

The build first compiles `tools/asmdata`, which extracts the original data segment from the ASM sources into
`build/generated/DGroup.{cpp,hpp}`.

## Game data

The original shareware data files are **not** in this repository (they are copyrighted). Extract the distributable game into
`assets/original/` so that `assets/original/IMG/3D.IMG` exists; for a packaged copy put the same folder as `assets\original`
next to the exe. The exe searches upward from its own folder. Tests that need the data skip themselves when it is missing.

## Playing

Run `corncob3d.exe`. The title sequence plays (any key skips it), then the main menu: arrow keys and Enter, or the
highlighted letter. `Training missions` is the quick way into a plane; `Duty roster` makes a pilot whose career is kept
(pilots, theaters, high score lists and settings live in your user data folder; `--save-dir` changes it).

The flight keys are those of `CORNCOB.DOC` (keypad 8/2 nose down/up, 4/6 roll, 7/9 or z/x rudder, keypad +/- throttle,
space guns, c missiles, b bomb, e eject, F1 boss screen, F2 the control tower's missions, F4 help ...). They can be changed
in `keyboard/Joystick setup` > `reDefine keys`. An Xbox-style gamepad works as well: left stick flies, right stick looks, `A` guns,
`X` missiles, `B` bomb, `Y` flaps, `Back` eject, `Start` pause (on foot: left stick walks, right stick looks).

The game paces itself at about 20 frames per second, because its fixed-point arithmetic only works at the original's frame
rate; `keyboard/Joystick setup` also has the high-resolution factor and the frame rate.

### Command line

```
--skip-intro  --no-music  --nosound  --hd <n>  --fps <n>  --save-dir <dir>
```

and for looking at one thing (the full list is at the top of `src/main.cpp`): `--theater <name>` / `--training` fly a mission
directly, `--screenshot <bmp> --frames <n> --keyscript "<frame>+<hex>;..."` run one unattended, `--ui-keys` / `--ui-shots` drive the
menus unattended, `--fake-mission`, `--spawn-enemies`, `--end-pictures`, `--space-shot`, `--play-rol`, `--wav-rol`.

## Layout

| Path | Contents |
|---|---|
| `*.ASM`, `*.INC`, `*.LNK`, `MAKEFILE` | Original source (the game exe is linked per `3S.LNK`, shareware) |
| `tools/asmdata/` | MASM data-segment extractor (build-time code generator); `tools/theaterstats` counts objects |
| `src/core/` | Bit-exact equivalents of the original arithmetic (`AsmMath`), the author's library routines (`SasLib`) |
| `src/engine/` | The 3D engine: `F3dVec`, `F3dObj` (collisions, object calls), tables |
| `src/game/` | The game: `GameState` (the original data segment), `Main3*` (the main loop, flight, weapons, enemies), `World`, `Keyboard`, `Inst`, `MissionRunner`, `TheaterFiles` |
| `src/audio/` | `Opl` (ymfm chip), `Sound` (the AdLib effects), `RolPlayer` (the music) |
| `src/render/`, `src/platform/` | EGA screen emulation, frame buffer, SDL window and input |
| `src/ui/` | Title and story, text screens, the menus (`FrontEnd`), pilots and scores (`Career`, `Scoring`), tower screens, mission-end pictures |
| `tests/` | ctest suites |
| `docs/PORT_PLAN.md` | The plan, what was done, deviations and what was left out |

## Notes on fidelity

- Arithmetic follows the 8086 code, including quirks (sign tests after `cmp`, 16-bit wraps, `rand_w` is a stand-in generator
  since `sas.lib` is not in the source). Divide overflows saturate where the 8086 would trap.
- The source tree is a later snapshot than the shipped `3.EXE`: it is built as `guns=1`, `slalom=0`, `getunivbin=0`.
- Decisions where the original is unclear or buggy are listed in `docs/PORT_PLAN.md`.

Corncob 3-D is © 1991-92 Pie in the Sky Software / MVP Software; the music is © James L. Collymore. This repository holds
the port's code, not the game's data.
