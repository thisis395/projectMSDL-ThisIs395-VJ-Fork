# ThisIs395 VJ Development Roadmap

This document records planned features, not claims that they are already implemented.

## Phase 1: PNG user sprites and FFT-driven animation (active)

- `Options > Sprite Manager...`: import transparent PNG images.
- Place, rotate, scale, and fade overlays independently of MilkDrop preset changes.
- React to projectM's existing smoothed bass, mid, and treble analysis.
- Support multiple managed sprites with an adjustable audio gain.
- Initial implementation uses libprojectM's MilkDrop user sprite API, not an additional FFT processor.
- Remaining before a stable release: validate live texture loading and transitions, persist sprite scenes, handle filename collisions robustly, and test rendering on each target operating system.

## Phase 2: Video sprite assets

- Decode video via FFmpeg (exact supported codecs/formats to be determined).
- Support embedded alpha when available; provide a configurable green/blue chroma-key shader (key color, tolerance, softness, spill suppression).
- Loop, cue, pause, and synchronize clips independently of preset transitions.
- Use a decoupled decode thread and GPU upload so 30/60 fps source video does not force projectM to 30/60 fps.
- Provide clip-level bass/mid/treble reactive transforms and opacity.

## Phase 3: Playlist / multitrack show sequencer

- Create a visual editor, with independent **preset**, **PNG sprite**, **video sprite**, and **cue/transition** tracks.
- Time clips, reorder, drag, trim, loop, mute, and override manually during live playback.
- Save and reopen show projects, preserve relative paths when possible, and relink missing assets.
- Drive preset changes through libprojectM's existing playlist API instead of replacing it.
- Define a consistent show clock and transitions that work during playback and during exports.

## Phase 4: Playlist > Export tab

A **real video-editor-style Export tab**, not simply a `Save Playlist` action.

### Export interface

- Output path and file name.
- Entire show or selected in/out range.
- Resolution: 1080p, 1440p, 4K, and custom dimensions.
- FPS: 30/60 first, evaluate 120+ once benchmarked.
- Default codec/container: H.264 MP4; optional H.265/HEVC, VP9/WebM, ProRes/MOV where available.
- Quality controls: bitrate/CRF or encoder-specific equivalent, software/hardware encoding selection.
- Audio-source selection, audio inclusion and muxing options.
- Render progress, cancellation, errors, completion report and output file.

### Rendering requirements

- The exported video includes **presets and composited sprite assets**, not ImGui menus or editor UI.
- For offline export, drive renderer time and FFT analysis from a chosen audio file at fixed output frame intervals.
- Investigate/implement a deterministic clock interface for libprojectM before promising frame-accurate results.
- Render offscreen via OpenGL FBO; encode/mux with FFmpeg. Avoid reliance on desktop screen capture.
- The MVP should export a selected 1080p60 show segment to H.264 MP4 with synchronized source audio.
- Treat recording live performances as a separate capture workflow; it cannot be assumed identical to deterministic offline export.
- Add transparent overlays or PNG image-sequence export later if practical.

## Phase 5: Qt 6 frontend

- Keep libprojectM rendering and the sprite/show backends independent of the UI toolkit.
- Consider Qt 6 Widgets for native windowing, file dialogs, and system menus.
- Provide macOS system menu integration, appropriate Windows and Linux menus, detachable controls.
- Do not start this rewrite until the Sprite Manager and sequencing/export interfaces have working backend contracts.

## Cross-platform requirements

- Windows x64, macOS Universal 2 (Intel and Apple Silicon), and Linux x64.
- Preserve unsigned/ad-hoc development artifacts, test results, and signed releases separately.
- GitHub Actions compilation alone is not proof of correct audio capture or interactive rendering.
