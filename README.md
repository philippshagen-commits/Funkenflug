# Funkenflug

A piano MIDI visualizer: falling / rising notes, sparks, glow, pedal and dynamics,
custom backgrounds, recording and mp4 rendering. Runs as a **standalone app** and as a **VST3 plugin**.

## Getting the Windows build

1. Create an empty repository on GitHub and upload the contents of this folder (keep the `.github` folder).
2. Open the repository's **Actions** tab. The "Build Windows" run starts on its own (or press *Run workflow*).
3. When it is green, download the **Funkenflug-Windows** artifact. It contains:
   - `Funkenflug.exe` – the standalone app
   - `Funkenflug.vst3` – copy this folder to `C:\Program Files\Common Files\VST3\`
   - `ffmpeg.exe` – keep it next to `Funkenflug.exe` (used for video rendering)

Building locally instead: install Visual Studio 2022 (C++ workload) and CMake, then
`cmake -B build -A x64` and `cmake --build build --config Release`. JUCE is downloaded automatically.

## Using it

**Live:** start `Funkenflug.exe`, press *Options* (top left) and pick your audio output and MIDI keyboard.
Play: notes rise from the keys. Velocity drives brightness and the number of sparks, the sustain pedal
keeps a thin trail on the notes it holds and lights the strip above the keys. You can also click the on-screen keys.

**Sound:** *Instrument* button
- *Load SoundFont or SFZ* – `.sf2` SoundFonts and `.sfz` instruments (for example the free
  [Salamander Grand Piano](https://github.com/sfzinstruments/SalamanderGrandPiano): unzip it and open
  `Salamander Grand Piano V3.sfz`). Works in the standalone app and in the plugin. *SFZ detail* trades memory
  for velocity layers (the full Salamander needs about 1.2 GB of RAM).
- *Load VST3 instrument* (standalone only) – the app hosts your piano VST3 itself, so one MIDI keyboard
  drives both sound and visuals.
- Without either, a simple built-in piano sound plays (Settings → Sound to switch it off).

**In a DAW:** put Funkenflug on an instrument track and send it the same MIDI as your piano track.
It makes no sound there unless you load a SoundFont. (LMMS cannot load VST3; use the standalone app.)

**Record / play:** *Record* captures what you play, *Stop rec* turns it into a take you can play back,
save as a MIDI file (*Save take*) or render. *Open* loads any MIDI file. *Live* switches back to the live view.

**Render video:** *Render video* → 720p / 1080p / 1440p / 4K, 30 or 60 fps, video quality, audio source
(none, the current instrument rendered offline, or an audio file) and AAC bitrate.

**Settings:** themes, colour wheel for every colour, colour modes, note gradient and shine, particle styles,
ripple shape / size, atmosphere (dust, snow, fireflies, embers, starfield, fog, light rays), bloom amount /
threshold / spread, keyboard looks, background image. Presets can be saved to and loaded from `.ffpreset` files.

Keys: Space play/pause · R record · S settings · O open · L live · F or F11 fullscreen · Esc cancel.

## Command line

`Funkenflug.exe --render in.mid out.mp4 [--size 1920x1080] [--fps 60] [--crf 18] [--theme N] [--sf2 file.sf2|file.sfz] [--bg image.png] [--no-audio] [--set id=value ...]`

## Licences

Built with JUCE 8 (AGPLv3) – this project is therefore AGPLv3. SoundFont playback uses TinySoundFont (MIT); the SFZ player is part of this project.
ffmpeg is a separate program under its own licence.
