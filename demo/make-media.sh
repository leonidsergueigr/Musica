#!/usr/bin/env bash
# Generates the copyright-free tracks used by demo/musica.tape.
#
# Everything is synthesized with ffmpeg: no external audio, no sampled
# material, and the artist/album names are fictional. The files are written to
# demo/media/ and are not tracked by git.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
out_dir="${1:-$here/media}"
duration="${MUSICA_DEMO_DURATION:-95}"

# Start from an empty directory so renamed tracks do not linger.
rm -rf "$out_dir"
mkdir -p "$out_dir"

# A gradient with a vinyl-like disc, so the 12x6 cover block is recognizable.
make_cover() {
  local path="$1" c0="$2" c1="$3" c2="$4" c3="$5"
  local fade='if(lt(hypot(X-256,Y-256),34),0.15,if(lt(hypot(X-256,Y-256),150),1,0.55))'
  ffmpeg -v error -y \
    -f lavfi -i "gradients=s=512x512:r=1:c0=$c0:c1=$c1:c2=$c2:c3=$c3,format=rgb24" \
    -vf "geq=r='r(X,Y)*$fade':g='g(X,Y)*$fade':b='b(X,Y)*$fade'" \
    -frames:v 1 -q:v 3 "$path"
}

make_track() {
  local artist="$1" title="$2" album="$3" c0="$4" c1="$5" c2="$6" c3="$7"
  local f1="$8" f2="$9" f3="${10}"
  local base="$out_dir/$artist - $title"
  local cover="$base.cover.jpg"
  local audio="$base.audio.mp3"

  make_cover "$cover" "$c0" "$c1" "$c2" "$c3"
  ffmpeg -v error -y \
    -f lavfi -i "sine=frequency=$f1:duration=$duration" \
    -f lavfi -i "sine=frequency=$f2:duration=$duration" \
    -f lavfi -i "sine=frequency=$f3:duration=$duration" \
    -filter_complex \
      "[0:a][1:a][2:a]amix=inputs=3:normalize=0,volume=0.10,tremolo=f=0.3:d=0.4,aecho=0.8:0.6:420:0.2,afade=t=in:d=2,afade=t=out:st=$((duration - 3)):d=3[a]" \
    -map "[a]" -c:a libmp3lame -b:a 128k "$audio"

  ffmpeg -v error -y -i "$audio" -i "$cover" -map 0:a -map 1:v -c copy \
    -id3v2_version 3 \
    -metadata title="$title" -metadata artist="$artist" \
    -metadata album="$album" \
    -metadata:s:v title="Album cover" -metadata:s:v comment="Cover (front)" \
    "$base.mp3"

  rm -f "$cover" "$audio"
}

make_track "Aurora Fields" "Morning Glass" "Soft Focus" \
  0x2b2d42 0x8e9eab 0xc9d6df 0x2b2d42 220.00 261.63 329.63
make_track "Bright Harbor" "Blue Current" "Night Tides" \
  0x1b1b3a 0x667db6 0xb8c6ff 0x1b1b3a 146.83 174.61 220.00
make_track "Mira Vale" "Paper Lanterns" "Small Hours" \
  0x6d2e46 0xe5989b 0xffd6d6 0x6d2e46 196.00 246.94 293.66
make_track "Quiet Orbits" "Blue Hour" "Low Orbit" \
  0x14213d 0x3a506b 0x9fb4c7 0x14213d 164.81 196.00 246.94
make_track "Sable Coast" "Driftwood" "Salt Air" \
  0x3a2e39 0xc9ada7 0xf2e0da 0x3a2e39 130.81 164.81 196.00
make_track "Studio Lumen" "First Light" "Daylight" \
  0x1b3a2b 0x90be6d 0xd9f2b0 0x1b3a2b 174.61 220.00 261.63

printf 'Generated %s\n' "$out_dir"
