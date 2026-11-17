# Musica

A fast, keyboard-and-mouse friendly terminal music player written in C++20.

**Français : [README.fr.md](README.fr.md)**

## Features

- Folder browser and play queue side by side
- Gapless-ish sequential playback, shuffle and repeat modes
- Live multi-line spectrum analyser
- Interactive seek and volume bars (click, drag, mouse wheel)
- M3U playlists: create, add, load, rename, delete
- French / English interface, detected from the locale
- Mouse support, and `--no-mouse` when you want to select text
- Small terminal friendly: a compact layout is used below 80×24

## Build

Requires a C++20 compiler, CMake ≥ 3.20, and the usual TagLib build
dependencies (zlib, cmake). FTXUI, TagLib and miniaudio are fetched or
vendored automatically.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/musica
```

## Configuration and data

- Playlists: `~/.config/musica/playlists/` (or `$XDG_CONFIG_HOME/musica/playlists/`)
- Interface language: `--lang fr|en`, or press `L` at runtime

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
| `p` / `P` | Manage playlists / add the current track |
| `?` | Help |
| `L` | Switch language |
| `q` | Quit |

Mouse: click to select, double-click to play, wheel to scroll, drag the
progress and volume bars.

## License

See `LICENSE` (to be chosen) and the third-party notices in the README.
