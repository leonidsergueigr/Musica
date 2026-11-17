# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-11-17

### Added

- Terminal interface with a folder browser and a play queue side by side.
- Sequential playback, shuffle and repeat modes, with play/pause,
  next/previous, seek and volume controls.
- Album art read from embedded tags or from `cover.jpg` / `folder.png` next to
  the track, loaded in a background thread.
- Multi-line real-time spectrum analyser.
- Mouse support for selection, playback, scrolling, seeking, volume and
  command buttons, plus `--no-mouse` to keep terminal text selection.
- Live `/` search in the active panel, ignoring case and accents.
- Track info popup and queue editing (remove, reorder).
- M3U playlists: create, add the queue, load, rename and delete with
  confirmation.
- English and French interface, chosen from the locale, with `--lang` and a
  runtime toggle.
- Persisted last folder, volume, shuffle, repeat and language in
  `$XDG_CONFIG_HOME/musica/state`.
- Support for MP3, FLAC, WAV, Ogg, M4A and Opus files; unreadable files are
  skipped with a visible message.
- Compact layout for terminals below 80x24 and a clear message below 40x12.
- `--help` and `--version` command-line options.
- Demo media generated with ffmpeg and a vhs recording script.

[Unreleased]: https://github.com/leonidsergueigr/Musica/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/leonidsergueigr/Musica/releases/tag/v0.1.0
