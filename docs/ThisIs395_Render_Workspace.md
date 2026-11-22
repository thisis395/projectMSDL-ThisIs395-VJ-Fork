# NLE-style render workspace (draft MVP)

The **Render** tab lives in the existing Playlist window, next to **Playlist + Sequence** and **Export**. It uses the same in-memory sequence items and edits their per-clip duration and fade.

## Working features

- Two time-scaled tracks, **Presets** and **Sprites**, with time ruler, zoom, playhead scrub, selection, and clip inspector.
- The existing live projectM application window acts as the program monitor. **Cue selected preset** sets the active preset in the live renderer, but is not a frame-accurate offline preview.
- Render settings: width, height, FPS, codec choice, bitrate, intended output path, optional source audio path.
- **Save render job (.json)** writes a UTF-8, human-readable **thisis395.render-job.v1** manifest, including ordered preset and sprite tracks, starts, durations, fades, paths, and output settings. Existing files are not overwritten.
- Sources and audio are entered as paths; no existence/decodability validation is performed yet.

## Explicitly not implemented

**No MP4/ProRes export occurs in this PR.** There is no background render queue, independent offscreen projectM context, deterministic frame clock, audio file decoding/synchronization, cross-platform FFmpeg encoder, or embedded video preview. Settings and render job files capture intended output only. UI modifications are session-only except for the exported JSON manifest.

## Timeline semantics and limitations

Presets are laid out end-to-end on the Presets track. Sprites are laid out end-to-end on their own track starting at 0; these independent lane starts are a first-pass NLE model and **not** yet backed by a shared show-time scheduler. Dragging and trimming with mouse are not implemented; use the Sequence editor's reorder controls and the Render clip inspector to edit timing. Saving a render job does not modify the playlist or export a video. There is currently no JSON import.

## Follow-up milestones

1. Extract shared, persistent sequence/timeline project model with explicit track time, trim, overlap, and undo.
2. Decode WAV/FLAC/other supported music to projectM PCM, advance with deterministic frame count, and render into an offscreen OpenGL framebuffer.
3. Synchronize sprite lifecycle and opacity against timeline, using Sprite Manager integration.
4. Capture RGBA frames and mux H.264/H.265/ProRes plus audio through FFmpeg, with error handling, progress, cancellation and hardware/software codec options.
5. Regression test on Windows, macOS, and Linux, including long recordings, audio drift and frame accuracy.
