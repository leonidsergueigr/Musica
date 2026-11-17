# Third-party licenses

Musica is distributed under the MIT license (see [LICENSE](LICENSE)). It uses
the following third-party components.

## miniaudio 0.11.22

- Project: <https://github.com/mackron/miniaudio>
- License: public domain (Unlicense) or MIT No Attribution, at your choice.

The full text of both alternatives is at the end of
`external/miniaudio/miniaudio.h`. This repository contains an unmodified copy
of the single-header library.

## stb_image 2.32

- Project: <https://github.com/nothings/stb>
- License: MIT or public domain (Unlicense), at your choice.

The full text of both alternatives is at the end of
`external/stb/stb_image.h`. This repository contains an unmodified copy of the
single-header library.

## FTXUI 7.0.3

- Project: <https://github.com/ArthurSonzogni/FTXUI>
- License: MIT, Copyright (c) 2019 Arthur Sonzogni.

FTXUI is fetched by CMake at configure time when `MUSICA_SYSTEM_DEPS` is
`OFF`. Its license text is available in the upstream repository as `LICENSE`.

## TagLib 2.1.1

- Project: <https://taglib.org/>
- License: GNU Lesser General Public License 2.1 or Mozilla Public License 1.1,
  at your choice.

TagLib is fetched by CMake at configure time when `MUSICA_SYSTEM_DEPS` is
`OFF`. The upstream license texts are `COPYING.LGPL` and `COPYING.MPL`.

Musica links TagLib statically in the default build. The complete source code
of Musica, including its CMake scripts and the pinned TagLib revision, is
published in this repository, so the corresponding source of the combined work
can be rebuilt with `cmake -B build && cmake --build build`.
