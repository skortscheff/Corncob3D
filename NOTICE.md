# Notices, provenance and licensing

This repository mixes four kinds of material. They are not all under the same terms.

## 1. The port (C++ code) — MIT

`src/`, `tests/`, `tools/`, `CMakeLists.txt`, `docs/` and the new text of `README.md` are
released under the MIT License (see `LICENSE`). The port is open to everyone: use it, study
it, fork it, ship it.

## 2. The original source code (assembly) — author's permission, no formal license

The MASM sources in the repository root (`*.ASM`, `*.INC`, `*.LNK`, `MAKEFILE`) are the
original Corncob 3-D source code by Kevin Stokes / Pie in the Sky Software, published by
MVP Software. They are included unchanged so the port can be checked against them and so
the build tool `tools/asmdata` can read them.

Provenance, as documented by the public mirror <https://github.com/foone/Corncob3D>:

- The source was released by its author in 2001 on psky.com. Archived page:
  <https://web.archive.org/web/20020108042105/http://psky.com/prog/downloads.htm>
- It was republished on 2018-04-11:
  <https://twitter.com/dosnostalgic/status/984236075945222146>
- The mirror records the original developer's permission to use the code freely
  ("do whatever you like with it"):
  <https://twitter.com/TideGear/status/984514063387344896>

That permission is informal: there is **no license file** and no copyright assignment. These
files are therefore **not** relicensed under MIT. They remain the work of their original
author, and are included on the strength of that public permission. If you are the rights
holder and want something changed or removed, please open an issue.

## 3. Third-party code — their own licenses

| Path | What | License |
|---|---|---|
| `third_party/ymfm/` | ymfm Yamaha FM sound chip emulator, © 2021 Aaron Giles, <https://github.com/aaronsgiles/ymfm> | BSD 3-Clause (`third_party/ymfm/LICENSE`) |
| (linked at build time) | SDL2, <https://libsdl.org> | zlib license |

## 4. Not included — game data and music

The original game's data (graphics, theaters, sounds, `.ROL`/`.BNK` music, documentation)
is **not** in this repository and is not covered by any of the licenses above.

- The shareware release is © 1991-92 Pie in the Sky Software / MVP Software. Its
  `LICENSE.DOC` permits non-commercial distribution of the *unmodified shareware package*
  only.
- The music is © James L. Collymore.
- "CORNCOB 3D" is a trademark of MVP Software.

To play, obtain the shareware release yourself and place it in `assets/original/` (see
`README.md`). Do not commit or redistribute those files through this repository.

## 5. Recreated, not original

The menu programs (MOAG, TOWER, ISCORE) have no source, so the front end is rebuilt from the
programs' text and the manual. The raw-score weights, rank thresholds and medal rules are
recreations. `rand_w` and the other `sas.lib` routines are inferred, because that library is
not in the source release.

## Disclaimer

This is an unofficial fan port. It is not affiliated with, endorsed by, or supported by Pie
in the Sky Software, MVP Software, or James L. Collymore.
