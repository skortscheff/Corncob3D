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

## Project status: an experiment, unsupported

This started as a weekend experiment. It comes **with no support whatsoever**: no promised bug fixes, no roadmap, no
guarantee that issues or pull requests will be answered. It will probably never be called a finished project. It works and
it is fun to play, but expect rough edges.

**Please take it and run with it.** The code is MIT licensed, and the work done so far is a foundation anyone can build on:
copy it, fork it, rewrite parts of it, port it to other platforms, fix what bothers you, add what you miss. You don't need
to ask permission or send anything back. Ideas that would be welcome in a fork:

- several airfields per theater (the original supports nine)
- Linux and macOS builds, tested
- more faithful scoring, ranks and medals (the current rules are recreations)
- joystick support, demo recording and playback
- anything else you can think of

If you make something good out of it, I'd love to hear about it, but that is entirely optional.

## Requirements

| | |
|---|---|
| **OS** | Windows 10/11 (the only platform built and tested). The code has no Windows-specific calls, so Linux and macOS should build with the same CMake steps, but that is untried. |
| **Compiler** | Any C++17 compiler. Tested with GCC from MSYS2 UCRT64. |
| **Tools** | CMake 3.16 or newer, Ninja (or any CMake generator), git |
| **Library** | SDL2 (development package) |
| **CPU / GPU** | Modest. The game is software-rendered at 640x350 (the original EGA mode), so no GPU features are needed. Any PC from the last 10-15 years should do; I have not measured minimums. A higher `--hd` factor and the 60 fps default cost more CPU. |
| **RAM / disk** | Well under 100 MB of RAM; the source, build and game data together are a few hundred MB at most. |
| **Input** | Keyboard (the original keys, rebindable), or an Xbox-style gamepad. |
| **Audio** | Any sound output. The AdLib chip is emulated in software. |
| **Game data** | The original Corncob 3-D shareware files, which you supply (see below). |

## Quick start (Windows)

1. **Install MSYS2** from <https://www.msys2.org>. Open the **MSYS2 UCRT64** shell and run once:

   ```sh
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-SDL2 git
   ```

2. **Get the code.**

   ```sh
   git clone https://github.com/skortscheff/Corncob3D.git
   cd Corncob3D
   ```

3. **Build** (in the same UCRT64 shell, or in PowerShell after `$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"`):

   ```sh
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

4. **Add the game data** (see below), then **run** `build/corncob3d.exe`.

5. **Check it works** (optional): `ctest --test-dir build`. All 13 suites should pass in a few seconds with the game data in place. Without it, the
   suites that need the data are reported as skipped.

To make a folder you can zip and hand out: `cmake --build build --target package` writes `build/dist/Corncob3D`
(the exe, `SDL2.dll` and this README).

**On Linux or macOS** (untested): install a C++17 compiler, CMake, Ninja and the SDL2 development package from your
package manager, then run steps 2-4 above.

If the build fails, check that `cmake` finds SDL2 (`find_package(SDL2)`); on MSYS2 that means building from the UCRT64
shell, not the plain MSYS shell.

## Game data

**You need the original game files to play, but not to build.** The code compiles, and the program builds, without them;
the files are read when the game runs. Only the tests that fly or load real game content need them too, and without them
those tests are reported as skipped. So build in any order, but add the data before you run the game.

The data files are **not** in this repository, because they are copyrighted. Obtain the Corncob 3-D **shareware release**
yourself (the original distribution, as a zip) and extract it so that this file exists:

```
assets/original/IMG/3D.IMG
```

The extracted folder should look like the shipped game: `IMG/`, `DEF/` (the theaters `DUSK`, `MORNING`, `WASTELND`,
`WHTSANDS`), `VGA/`, `THT/`, the music files (`*.ROL`, `CCMUSIC.BNK`), `3.ADL` (the AdLib sound data), the text documents
(`CORNCOB.DOC` and others), and so on. You can also copy the original `3.EXE` and the other programs along with the rest;
the port does not run them.

**Where the game looks.** The exe looks for `assets/original/IMG/3D.IMG` starting in its own folder and then in up to five
parent folders, and also relative to the folder you launch it from. That is why `build/corncob3d.exe` finds the data in the
repository's own `assets/original/`, and why a packaged copy works with the data placed in an `assets/original` folder next
to the exe (`cmake --build build --target package` creates that folder with a note in it).

**Keep it out of git.** `assets/original/` is listed in `.gitignore`. Please don't commit or redistribute those files
through this repository.

**If the game won't start properly,** the usual cause is missing data. Check that `assets/original/IMG/3D.IMG` exists
in one of the places above. (The port does not yet show a friendly "game data not found" message.)

## How this port came about

I played Corncob 3-D as a kid on my DOS PC, and recently stumbled upon it again. Around the same time I enrolled in a
Claude certification course, and this project grew out of that: a chance to put what I was learning to work on a game
I had never forgotten.

Corncob 3-D (1991-92, Pie in the Sky Software / MVP Software) was written in 8086 assembly for DOS, talking straight to
EGA/VGA hardware, the AdLib chip and the PC keyboard interrupt. Its author later released the source, and it survived on
the Wayback Machine and a public GitHub mirror (see `NOTICE.md`). The source is a snapshot that does not match the shipped
`3.EXE` exactly, and the menu programs and the author's `sas.lib` were never part of it.

The port was done over about five days (14-18 October 2026) as a pair-programming project between a human and Claude Code, an AI
coding assistant, working from a written plan of ten phases. The key decisions:

- **Translate, don't rewrite.** Each assembly routine became a C++ routine with the same name, order and fixed-point
  arithmetic, quirks included. A build-time tool (`tools/asmdata`) reads the assembly's data definitions and generates the
  original data segment, so the translated code keeps working on the same memory layout.
- **Emulate the hardware, not the PC.** EGA bit planes, the AdLib chip (through the ymfm emulator), and the keyboard
  interrupt are emulated, rather than running the original code in an x86 emulator.
- **Test against the original.** The shipped `3.EXE` ran in DOSBox as a reference, and the tests replay flights over the
  theaters, check arithmetic against the 8086 behavior, and drive the menus end to end.
- **Rebuild what was missing.** The menu programs, high-score tables and tower screens were recreated from their text
  data and the manual.
- **Modernize gently.** A 60 fps time base that keeps the original's feel, an HD scale option, a modern instrument panel
  (`--classic-panel` restores the original), key rebinding and gamepad support.

`docs/PORT_PLAN.md` is the full phase-by-phase record, including every place where the port deliberately differs from the
original.

## Playing

Run `corncob3d.exe`. The title sequence plays (any key skips it), then the main menu: arrow keys and Enter, or the
highlighted letter. `Training missions` is the quick way into a plane; `Duty roster` makes a pilot whose career is kept
(pilots, theaters, high score lists and settings live in your user data folder; `--save-dir` changes it).

The flight keys are those of `CORNCOB.DOC` (keypad 8/2 nose down/up, 4/6 roll, 7/9 or z/x rudder, keypad +/- throttle,
space guns, c missiles, b bomb, e eject, F1 boss screen, F2 the control tower's missions, F4 help ...). They can be changed
in `keyboard/Joystick setup` > `reDefine keys`. An Xbox-style gamepad works as well: left stick flies, right stick looks, `A` guns,
`X` missiles, `B` bomb, `Y` flaps, `Back` eject, `Start` pause (on foot: left stick walks, right stick looks).

The simulation runs at 60 frames per second by default (a reworked time base keeps flight, weapons and timers at the original's pace);
`keyboard/Joystick setup` switches to the original's 20 and sets the high-resolution factor.

### Command line

```
--skip-intro  --no-music  --nosound  --hd <n>  --fps <n>  --classic-panel  --save-dir <dir>
```

and for looking at one thing (the full list is at the top of `src/main.cpp`): `--theater <name>` / `--training` fly a mission
directly, `--screenshot <bmp> --frames <n> --keyscript "<frame>+<hex>;..."` run one unattended, `--ui-keys` / `--ui-shots` drive the
menus unattended, `--fake-mission`, `--spawn-enemies`, `--end-pictures`, `--space-shot`, `--play-rol`, `--wav-rol`.

## Quality-of-life enhancements

Beyond a faithful port, a few things were added so it plays comfortably on a modern PC. Where it makes sense, they can be switched
back to the original behavior.

- **60 fps simulation.** The original ran at about 20 frames per second. The port runs at 60 by default, with flight,
  weapons and timers rescaled so the plane still handles like the original. 20 fps is available in
  `keyboard/Joystick setup` or with `--fps 20`.
- **Xbox-style gamepad.** Left stick flies, right stick looks around, `A` guns, `X` missiles, `B` bomb, `Y` flaps, `Back`
  eject, `Start` pause. On foot after ejecting, the left stick walks and strafes and the right stick looks.
- **HD scaling.** Missions can be drawn at 1x to 4x the original 640x350 (`--hd <n>` or the setup menu), keeping the EGA look.
- **Modern instrument panel.** Anti-aliased dials, lamps and bars over the EGA picture. `--classic-panel` restores the
  original panel.
- **Rebindable keys.** `keyboard/Joystick setup` > `reDefine keys`; bindings are saved in your settings.
- **Persistent pilots and settings.** Careers, high scores and options are kept in your user data folder
  (`--save-dir` changes it).
- **No DOS setup needed.** No `CONFIG.SYS` tweaking, no sound card IRQs, no CPU-speed throttling. It runs in a normal
  window, and AdLib sound and music are emulated.

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

## License and credits

- The port's code (`src/`, `tests/`, `tools/`, `docs/`, build files) is **MIT licensed** — see `LICENSE`. Contributions are welcome.
- The original assembly source in the repository root is by Kevin Stokes / Pie in the Sky Software. Its author publicly
  permitted free use of it; it is not relicensed here. Source: the [foone/Corncob3D](https://github.com/foone/Corncob3D)
  mirror, originally released in 2001 ([Wayback snapshot](https://web.archive.org/web/20020108042105/http://psky.com/prog/downloads.htm)).
- [ymfm](https://github.com/aaronsgiles/ymfm) (BSD 3-Clause, © Aaron Giles) emulates the AdLib chip; SDL2 (zlib) handles window, input and audio output.
- Corncob 3-D is © 1991-92 Pie in the Sky Software / MVP Software; the music is © James L. Collymore. This repository holds
  the port's code, not the game's data, which you must supply yourself.

Full details, including what is deliberately not included, are in `NOTICE.md`. This is an unofficial fan project, not
affiliated with the original authors or publisher.
