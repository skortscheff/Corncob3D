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
- **Phase 5** combat and timed objects (`STRTEXP.ASM`, parts of `DEFOBJS.ASM`, `3.ASM`, `PICT.ASM`), finished:
  - **5a** (`Main3Timed.cpp`): `objmov`, `expmov` (prime + move in one loop, as the original), flotsam, shards,
    incendiaries, puffs, booms with sound, the destroy paths `fdest`, `fdestrdr`, `gmdest`, `odest`.
  - **5b** (`Main3Weapons.cpp`): guns (`bulstrt`, `bulmov`, `bdest`), missiles (`strtmis`, `mistim`), bombs
    (`strtbomb`, `strtbomb1`, `bombtim`, the planted assassin bomb), craters (`crtrtim`).
  - **5c** (`Main3Sites.cpp`, `Main3Flyers.cpp`): AA guns and shells, death balls and their sites, vehicles and
    their sites, mortars, walls, barrage balloons, the headquarters orbs and every objective check (`bigdest`,
    `hqtim`, `dhqtim`, `incondest`), bombers and their sites, tetrahedra, aliens, portals, shell objects, the
    man, the hut, the rescue van start, and `initnumbers`. All 61 entries of `objtimtable` are wired in
    `Main3Objects.cpp`. Radar dots are drawn by `Instruments::radardot`.
  - **5d**: `Main3::finishResults` / `resultBytes` (the 3.res block the tower reads), the mission-end pictures
    (`ui/MissionEnd`, `ui/MissionPictures`, which follow `PICT.ASM pict` exactly), and the red crash screen.
  - Checked by ctest (`flight`: weapons, explosions, every enemy type running, a 6000-frame invincible soak over
    MORNING, DUSK and WASTELND; `pict`) and by screenshots.
  - Dev aids in `main.cpp`: `--spawn-hit <n>`, `--spawn-enemies`, `--end-pictures <hex> [--death n] [--shot-end p]`,
    `--res <file>`.
  - Deliberate deviation: the original's `bulmov` loops over 32 records of a table that has 23, walking into the
    templates after it; the port only moves the 23. `bdest` in the original tests a flag left over from the
    caller; it is always clear there, so the port skips the test.
  - Quirks kept: `nflt` is never incremented in the original (so the flotsam cap never applies); the first
    bullet record starts "live" and puffs once at its fixed spot.
- **Phase 7** AdLib sound effects through ymfm, and the music: `audio/RolPlayer` plays the original `.ROL` songs with
  `CCMUSIC.BNK` instruments on the same OPL core (percussive rhythm voices included). Formats are documented in
  its header; `test_rol` checks all three songs parse, every instrument resolves, and note pitches are right.
  `--play-rol FILE.ROL` and `--wav-rol FILE.ROL out.wav seconds` are dev aids. The title and menu music is played by
  `ui/MusicBox` (below).
- **Phase 8** the recreated front end (MOAG, TOWER and ISCORE have no source: their text and the manual are the spec):
  - `ui/TextScreen`, `ui/Widgets`: an 80x25 colour text screen and the menus, boxes, line entry and pager the
    original's screens are made of (`\x` hot key marks and `\(...\)` highlights of the original's text are
    understood). `GameText` reads `MOAG.INF` (the information pages) and `CORNCOB.DOC` (the documentation).
  - `ui/FrontEnd`: the main menu (duty roster, combat missions, training missions, high score lists, Read This,
    keyboard/joystick setup, music toggle, documentation viewer, information, exit), the training mission setup
    (numbers and wickedness of enemies, invulnerable plane), the combat mission flow (open, activate, delete and
    view theaters, airfield and mission briefing, flying, the mission report, promotions and medals), the duty
    roster (create, activate, deactivate, delete, resurrect, rename, view records).
  - `ui/Career`, `ui/Scoring`: pilots, theaters (a copy of the universe per pilot theater, so what is destroyed
    stays destroyed), the three score lists, ranks and medals, saved as `career.dat` and `theater<N>.cct` in
    the save directory (`ui/Settings`: `settings.cfg`, `--save-dir`, `CORNCOB_SAVE`). The score follows the manual's
    three examples (5*5*1, 4*1*1/2, 5*1*1 times a raw score); the raw-score weights, rank thresholds and medal
    rules are a recreation, the original's numbers are not in the files.
  - `game/MissionRunner`: one mission as a function (the options of `CMDLINE.ASM` come in a `MissionRequest`, the
    result block, status and, for combat missions, the saved universe come back), `game/TheaterFiles` (theater
    lists and objective counts from the `.CCT`, missions and briefings from the `.TWR`), `World::writeuniv`.
  - `ui/TowerScreen`: F2 or the tower door (the airfield's missions; choosing one sends the plane there, walking
    in repairs the plane), F1 the boss screen, F3/F6 the shareware "not available" messages (`Main3::dotower`).
  - `ui/MusicBox`: the title song (DESTINY3) and the menu songs (ROLFILES.TXT) from a thread of its own.
  - Checked by ctest: `career` (scoring examples, pilots, medals, lists, files), `theaters` (what the shipped theaters
    hold), `frontend` (the menus driven by scripted keys end to end) and by screenshots.
  - Dev aids: `--ui-keys`, `--ui-shots`, `--fake-mission`, `--tower-test` (see the header of `main.cpp`).
  - Left out: printing the documentation, the joystick calibration and the first-run screens (nothing to
    decompress, no DOS), the old registration menu, the sound editor. Only one airfield per theater is
    handled (the shipped theaters have one).
- **Phase 9** `README.md`, a `package` build target, key redefinition (`DosKeyboard::setBindings`, saved in `settings.cfg`), the high
  resolution factor and frame rate in the setup menu, and the lost-in-space sequence (`ui/SpaceSequence`, after SRATS.ASM:
  the sky fades while the Earth shrinks and the stars stream by).
- **Extra** Xbox pad: dual-stick scheme for flying and for walking (strafe, look).

## Remaining
Nothing in the plan is open. Left as it is, on purpose:
- Several airfields per theater (the original supports nine; the shipped theaters have one, so the airfield choice screens
  are not needed). The text of `.TWR` files with more towers is read the same way, but only the first airfield is used.
- Hand checks that tests cannot do: the music by ear, the feel of weapons and enemies, the menus with a pad. (Music and
  combat were played and felt right.)
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
- **60 Hz engine (deliberate deviation).** The original steps at ~20 fps and its fixed-point math only works there. The port
  runs at 60 (default) or 20 (`fps` setting / `--fps`). The reference frame is the 20 fps frame (58 timer ticks); at other
  rates: the time base is one tick (`ofrmticks`, `ticksf`=40) instead of 8 ticks; velocity/angle/position increments carry
  their fractions between frames and the flight velocity multiplies round (`velmul`); `wtadj` dithers its remainder;
  per-frame filters, kicks, drags and spins are scaled by `frameScale`/`decayStep`/`dispScale`; countdown lifetimes
  (`+52`), the swing counter, wall animation and start-up timers count `refSteps` (20 fps frames passed) and the
  lifetimes set at spawn are constants in those frames (`uwtadjRef`). Below 43 ticks per frame the angles come out of the
  matrix with an exact arctangent (`atn2Fine`) instead of the 8 unit table. At 20 fps the original paths are kept.
  `tests/test_rate` flies one script at 20 and 60 and compares speed, distance, climb and pitch. 30 fps is not offered (it
  climbs steeper than 20 and 60); on-ground nose-up at 60 is ~1 s slower than at 20.

## Step 0 results (2026-10-17)

**DOSBox reference.** A portable DOSBox Staging 0.83.0 is unpacked in `tools/dosbox/` (gitignored, local only).
`run/orig/` (gitignored) holds a copy of the original game with MORNING as `3univ.dat` / `towers.dat`, and
`run/dosbox.conf` mounts it and runs `3 -k -s` (`-k` = keyboard only; without it the game asks for joystick
calibration). The original starts at the same airstrip view as the port. Captures of just the DOSBox window
are taken with `PrintWindow` on its window handle (no whole-screen capture). Sending keys needs the DOSBox
window focused, so scripted comparisons will need a focus-safe way of doing that; not solved yet.

**Object types per shipped theater** (`tools/theaterstats`, which decodes every tile of the `.CCT`):

| id | template | routine | MORNING | DUSK | WHTSANDS | WASTELND |
|---|---|---|---|---|---|---|
| 27 | gms | gmdest | 35 | 24 | | 22 |
| 23 | fltsm1aa | aatim | 17 | 21 | | 17 |
| 13 | objvest | vesttim | 19 | 21 | | 8 |
| 11 | objdbst | dbsttim | 13 | 11 | | 15 |
| 24 | fltsm1dball | dballtim | 13 | 4 | | 6 |
| 12 | objvech | vechtim | 1 | 4 | | 9 |
| 42 | objmort | mortim | 15 | 6 | | |
| 14-17 | objwall* | walltim | 18 | 1 | | 20 |
| 45-47 | objlwall* | lwalltim | 16 | | | 1 |
| 49 | objshell | shelltim | 9 | 15 | | 26 |
| 52 | objtet | tettim | 16 | 6 | | 6 |
| 31 | objsurv | survtim | 5 | 5 | 5 | 5 |
| 40/41 | objbb / objbbsm | bbtim / bbsmtim | 5 | | | 1 |
| 50/53 | objbmr / objbmrmo | bmrtim | 2 | | | 2 |
| 57 | objal | altim | 6 | | | 1 |
| 32 | crtr | crtrtim | | | | 5 |
| 43 | objdhq | dhqtim | | | | 1 |
| 58 | objxp | xptim | | 2 | | 1 |
| 60 | objpstep | | | 2 | 5 | 2 |

WHTSANDS is the small one: only the field (road, tower, hut, survivors, men, shards, steps), no enemies.

**Order for 5c**, by how many theaters use a type and how many there are: ground missile sites (gms), AA
(aatim), vehicle sites and vehicles (vesttim, vechtim), death-ball sites and balls (dbsttim, dballtim,
radar), mortars, walls, shells and tetrahedra, then the one-offs (bb, bomber, altim, creature, HQ, portal).
