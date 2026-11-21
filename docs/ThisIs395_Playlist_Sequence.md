# Playlist and Sequence MVP

## Playlist tab
- Preserve the existing live playlist browser, search-free list, add/remove and playback behavior.
- Each preset row has a **Star** toggle (session-only favorite marker) and **+** button to append the preset to the sequence.

## Sequence tab
- Two sub-tabs: **Presets** and **Sprites**, each with a duration field per clip.
- Default duration is configurable, and clips can be moved up/down or removed.
- The **Play preset sequence** button plays preset entries via the existing libprojectM playlist API and an SDL clock.
- Sprite entries can be staged and edited, but **cannot render or be scheduled as active sprites yet**. This requires the Sprite Manager integration from PR #2. Sprite entries are skipped during preset-only playback.
- Editing or hiding the Playlist window may stop sequence timing; there is no standalone background show scheduler yet.
- All state is session-only.

## Export tab
- Export the **live preset playlist** as a UTF-8 `.m3u8` text file containing the ordered preset paths.
- Specify a complete output path; existing files are not overwritten.
- This is **not** video export, and is **not** sequence timeline serialization. Offline MP4 rendering requires a deterministic render clock, audio feeding, offscreen OpenGL and encoding.

## Next milestones
- Durable sequence project storage and playback scheduler independent of the GUI.
- Sprite Manager integration and overlapping sprite/preset tracks.
- M3U import and preset validation.
- Frame-accurate export with audio and FFmpeg.
