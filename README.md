# Clipcutter
A humble, simple video editor written in c-style c++ using SDL3, Dear ImGUI, LibMPV and the FFmpeg c api.

Under development, not suitable for usage. Testing and submitting issues is welcomed though

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
* Adjust gain of tracks
* Audio envelopes (about 150% faster generation than premiere, though admittedly lower resolution)
* Fast workflow
* * Drag video files to the Clipcutter window to open them
* * Register clipcutter to the "Open with" context menu to quickly get the editor up and running
* * Keyboard shortcuts
* * Fast startup and loading times (<1 sec to load the program and a video file ready to be played)
* UI Scaling support
* Clean, intuitive UI

#### Limitations
* Some of the 168 audio filters loaded from ffmpeg may not work because they require more complex chaining than is supported ATM. I might make a whole node system for this in the future.
* No effects on video
* Audio and video tracks cannot be seperated and you may not have multiple things playing at once. The scope of the project (at least for now) is cutting videos/audio and adding effects.
* You can only export video in h.264 (with AAC audio) at the moment. Audio can only be exported as mp3.

## Building
Like any other cmake project. Dependencies are all included, except ffmpeg dlls. In the future dependencies will be compiled along the program.

## Bug reports
Make an issue and try to provide steps on how to reproduce.

## Contributing
Contributions are welcom, though make an issue or contact me first so you don't do work for nothing. Also try to keep the code mostly C-style.
