# Terminal Trove demo for Musica.
#
# Setup (once):
#   - build the project:      cmake -B build && cmake --build build
#   - put a few tagged MP3s in /tmp/musica-demo
#   - install vhs, ttyd and ffmpeg
# Then:
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
Type "cd /tmp/musica-demo && XDG_CONFIG_HOME=/tmp/musica-demo-config LANG=en_US.UTF-8 /home/leonidgr/musica/build/musica --no-mouse"
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
Type "sad"
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
