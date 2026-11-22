# ThisIs395 VJ Playlist Editor

## First milestone: Live playlist

Open **Playlist > Playlist Editor...** to browse the current projectM preset playlist. Select and double-click a preset to play it, or press **Play selected**. You can toggle instant transitions, remove a preset from the running playlist, or add a preset using its full `.milk` filesystem path. The list is virtualized to avoid drawing every entry each frame.

Edits currently affect **only the running playlist**. They are not saved to disk and will be lost at restart. Shuffle changes in this view are likewise session-only and are not written to user configuration.

## Export tab

An explicit placeholder for the planned video-editor-style **Export** tab. **No video exporting is implemented in this PR.** The long-term pipeline requires a proper show clock, deterministic audio/FFT frame stepping, offscreen render targets, and FFmpeg encoding with audio muxing.

## Follow-ups

1. Save/load show projects and playlist order, with missing-asset relinking.
2. Timeline tracks for preset, sprites, video, and cues, including trim, reorder, and durations.
3. Synchronize scene scheduling and sprites with a common show clock.
4. Implement offline video export with source audio and progress/cancellation.
5. Verify multi-platform packaging and runtime UI behavior.

This PR intentionally targets `master` and does not depend on the Sprite Manager PR.
