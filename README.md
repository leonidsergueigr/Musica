# Musica

A fast, keyboard-and-mouse friendly terminal music player written in C++20,
with album art, a live spectrum and a folder browser.

**Français : [README.fr.md](README.fr.md)**

## Features

- Folder browser and play queue side by side
- Gapless-ish sequential playback, shuffle and repeat modes
- Album art from embedded tags (MP3, FLAC, M4A, Ogg) or `cover.jpg` /
  `folder.png` next to the files, loaded in the background
- Live multi-line spectrum analyser
- Interactive seek and volume bars (click, drag, mouse wheel)
- Live `/` search in the active panel (case- and accent-insensitive)
- Track info popup, queue editing (remove, reorder)
- M3U playlists: create, add, load, rename, delete with confirmation
- French / English interface, detected from the locale or saved choice
- Unreadable files are skipped with a visible message
- Mouse support, and `--no-mouse` when you want to select text
- Remembers the last folder, volume, shuffle, repeat and language
- Small terminal friendly: compact layout below 80×24, clear hint below 40×12

## Build

Requires a C++20 compiler, CMake ≥ 3.20, and the usual TagLib build
dependencies (zlib, cmake). FTXUI, TagLib and miniaudio are fetched or
vendored automatically.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/musica

# optional: install the musica binary
sudo cmake --install build
```

A screenshot or GIF can be recorded with [vhs](https://github.com/charmbracelet/vhs)
and placed in the README once captured.

## Configuration and data

- State (last folder, volume, shuffle, repeat, language):
  `~/.config/musica/state` (or `$XDG_CONFIG_HOME/musica/state`)
- Playlists: `~/.config/musica/playlists/` (or `$XDG_CONFIG_HOME/musica/playlists/`)
- Interface language: `--lang fr|en` (wins over the saved choice), or press `L`
  at runtime
- `--no-mouse` disables mouse tracking, `--help` and `--version` print CLI info

## Keys

| Key | Action |
| --- | --- |
| `␣` | Play / pause |
| `n` / `b` | Next / previous track |
| `←` / `→` | Seek -5 s / +5 s |
| `+` / `-` | Volume |
| `s` / `r` | Shuffle / repeat |
| `j` / `k` | Move the selection |
| `h` / `l` | Focus the browser / queue |
| `⏎` | Play or enter |
| `a` / `A` | Add a file / folder |
| `/` | Search the active panel (`esc` clears) |
| `i` | Track info |
| `x` | Remove the selected queue track |
| `J` / `K` | Move the selected queue track down / up |
| `p` / `P` | Manage playlists / add the current track |
| `?` | Help |
| `L` | Switch language |
| `q` | Quit |

In the playlists popup: `⏎` load, `a` add the queue, `s` save the queue,
`r` rename, `d` delete (with a y/n confirmation).

Mouse: click to select, double-click to play, wheel to scroll, drag the
progress and volume bars, click command chips and buttons.

## Third-party notices

See [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) for the components and
licenses bundled or fetched by the build:

- [miniaudio](https://github.com/mackron/miniaudio) — public domain / MIT-0
- [FTXUI](https://github.com/ArthurSonzogni/FTXUI) — MIT
- [TagLib](https://taglib.org/) — LGPL-2.1 / MPL-1.1
- [stb_image](https://github.com/nothings/stb) — public domain / MIT

## License

Musica is released under the [MIT license](LICENSE).
