# Renderer fixes (PR #6)

- Audio from the NLE track is decoded by FFmpeg to 48 kHz stereo float PCM in a temporary file. The native projectM loop feeds an aligned PCM window into projectM before each video frame; the same input is muxed into the video output.
- The native backend now creates projectM user sprites for PNG clips on the sprite timeline, controls opacity/fade and destroys them after their active interval. Sprites use projectM bass FFT expressions.
- Python encoder checks exact raw RGB byte count, waits for both processes, checks nonempty output and cleans up temporary PCM and partial outputs on failure.
- `--status path.json` reports preparing_audio, rendering (0–1 progress), completed, cancelled, failed; `--cancel path` is a cancellation sentinel.
- The Render tab displays a progress bar and has a Cancel button, polls the status JSON, and overwrites **only the render-job manifest** when exporting repeatedly. Existing finished video outputs are protected.
- Linux CI has a synthetic FFmpeg smoke test, verifying muxed video+audio and completed status. It does not substitute for actual GPU/sprite runtime tests.

Remaining: full hardware-accelerated offscreen tests on Windows/macOS/Linux, consistent GL context behavior, high-resolution performance, native UI, and robust crash recovery if a process is terminated without updating status. The GUI currently reports generic failure details with the status JSON path.
