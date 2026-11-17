# Terminal Trove demo for Musica.
#
# Setup (once):
#   - build the project:      cmake -B build && cmake --build build
#   - generate demo audio:    ./demo/make-media.sh   (requires ffmpeg)
#   - install vhs, ttyd and ffmpeg
# Then, from the repository root:
#   vhs demo/musica.tape

Output demo/musica.gif

Set Shell "bash"
Set FontFamily "Liberation Mono"
Set FontSize 15
Set Width 1200
Set Height 760
Set Padding 24
Set Theme "Catppuccin Mocha"
Set Framerate 30
Set TypingSpeed 60ms

Hide
Type "rm -rf /tmp/musica-demo /tmp/musica-demo-config && cp -r demo/media /tmp/musica-demo && cd /tmp/musica-demo && XDG_CONFIG_HOME=/tmp/musica-demo-config LANG=en_US.UTF-8 $OLDPWD/build/musica --no-mouse"
Enter
Sleep 1.2s
Show

# Browsing the folder, then playing the first track.
Sleep 1.5s
Down
Sleep 400ms
Enter
Sleep 4s

# Next track: new cover, new metadata, same queue.
Type "n"
Sleep 3.5s

# Live search over the browser panel.
Type "/"
Sleep 600ms
Type "blue"
Sleep 1.8s
Escape
Sleep 800ms

# Info popup for the selected (playing) track.
Down
Sleep 300ms
Type "i"
Sleep 3s
Escape
Sleep 500ms

# Keyboard help.
Type "?"
Sleep 2.5s
Escape
Sleep 500ms

# Back to the player, pause and resume on the current track.
Space
Sleep 1s
Space
Sleep 2.5s
