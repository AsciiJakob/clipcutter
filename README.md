# Clipcutter
A humble, simple video editor written in c-style c++ using SDL3, Dear ImGUI, LibMPV and the FFmpeg c api.

Under development, not suitable for usage.

## Features
* Video playback
* Cutting/trimming
* Video concatenation
* Exporting
* * exporting video and audio
* * exporting video only
* * exporting audio only
* * muting cerain audio tracks
* FFMPEG Audio effects (applies to all audio tracks, im interested in trying out a node-based system for effects in the future)
* Clip audio waveform preview (coming soon)
* Fast workflow
* * Drag video files to the Clipcutter window to open them
* * Register clipcutter to the "Open with" context menu to quickly get the editor up and running
* * Keyboard shortcuts
* * Fast startup and loading times
* UI Scaling support
* Clean, intuitive UI (unimplemented)
* TODO
* TODO
* TODO

#### Limitations
* You can only export video in h.264 (with AAC audio) at the moment. Audio can only be exported as mp3.
* You can only export video in h.264 (with AAC audio) at the moment. Audio can only be exported as mp3.

## Building
Like any other cmake project. Dependencies are all included, except ffmpeg dlls. In the future dependencies will be compiled along the program.

## Bug reports
Make a file explaining the bug and save it in /dev/null (you can also make a github issue)

## Contributing
