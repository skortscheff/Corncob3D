# Corncob 3-D port: status and remaining plan

Faithful port of the DOS game (MASM source in this repo) to C++17 / SDL2. Original assets stay local
(`assets/original/`, never committed). Updated 2026-10-17.

## Done
- **Phase 0** foundations: tests, `AsmMath`, `GameState` (the whole original data segment), `tools/asmdata`.
- **Phase 1** 3D engine (`F3DVEC`, `MAT`, clipping, fills) with the HD option.
- **Phase 2** world: theater files (`.CCT` = `3univ.dat`, `.TWR` = `towers.dat`), tiles, memseg.
- **Phase 3** the main loop (`3.ASM`): flight model, keyboard emulation, collisions, plane/tower/runway
  routines, map mode, remote tail view, training-mission start (`initperms`).
- **Phase 4** instruments, buttons, damage bars, help screens.
- **Phase 7 (part)** AdLib sound effects through ymfm.
- **Extra** Xbox pad: dual-stick scheme for flying and for walking (strafe, look).

## Remaining

### Step 0: verification setup
- Install DOSBox and run the shipped `3.EXE` as a timing/behaviour reference.
- Count the object types in the four shipped theaters (MORNING, DUSK, WHTSANDS, WASTELND) to rank which
  enemies a shareware mission uses.

### Phase 5: combat and timed objects (`STRTEXP.ASM`, about 5,300 lines), in four steps
- **5a timed-object engine and explosions:** `objmov` / `expmov`; flotsam (`strtflt`, `flttim`), shards,
  incendiaries, booms and puffs; fill in the Phase 3 destroy paths (`fdest`, `odest`, `funeral5d`, crash
  and wreck visuals). Done when a crash or a hit shows debris, fire and booms with sound.
- **5b your weapons:** guns (`bulstrt`, `bulmov`, `shelltim`), missiles (`strtmis`, `mistim`), bombs
  (`strtbomb`, `bombtim`); hit detection and damage; ammo counters.
- **5c enemies and mission objects**, ordered by the theater count: ground missile sites and AA, death
  balls and sites, vehicles and sites, survivors, bombers, the creature, walls, HQ, mortars, rescue.
- **5d scoring and results:** `initnumbers`, mission-status bits, `PICT.DAT` end screens, the crash screen.

### Phase 7 remainder: music
- ROL + BNK player for `CCMUSIC.BNK` / `*.ROL` (title and menu music) on the existing OPL core.

### Phase 8: recreated front end
- MOAG-like menus: main menu, duty roster (`PILOT.HSC/SCR`), combat and training mission selection,
  music toggle, Read This; tower briefings (`TOWER.EXE`); high scores (`ISCORE.EXE`); the
  `CORNCOB.BAT` loop (exit code back to the menu).
- Control setup: key bindings, the `CMDLINE.ASM` switches, a gamepad invert option.
- `--theater` and `--training` become launcher choices.

### Phase 9: polish and packaging
- Settings persistence, a `dist/` packaging target, performance pass, README.
- The `SRATS` space sequence (a separate program that runs when you fly too high) goes last.

## Dropped
- Real-joystick support (`JOY.ASM`): the pad covers it.
- Demo record/playback: the shareware build (`shareflag=1`, `nodemo=1`) excludes it.

## Verification
- ctest suites for each ported piece.
- Scripted key runs (`--keyscript`) with screenshots and state reports (replaces demo replay).
- Side-by-side comparison against DOSBox at the end of each of 0, 5a-5d.
- Every step ends with a playable build and a hand check of its features.

## Build notes
- The source tree is newer than the shipped `3.EXE`; built as `guns=1`, `slalom=0`, `getunivbin=0`.
- MSYS2 UCRT64 toolchain: `cmake --build build`, `ctest --test-dir build`.
- The simulation is only stable at about 20 frames per second (fixed-point matrix math), so the game
  paces itself to that.
