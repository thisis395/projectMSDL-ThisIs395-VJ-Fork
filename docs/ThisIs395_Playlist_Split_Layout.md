# Split playlist and sequence editor

The **Playlist + Sequence** view now places the sequence on the left and the live preset library on the right, following the referenced ProjectMilkSyphon layout.

- On the right, **Star** toggles session favorites and **+** appends a preset to the sequence.
- On the left, **Presets** and **Sprites** tabs show the ordered sequence entries and duration controls.
- Default **Preset duration** and **Fade duration** are editable; preset sequence clips have their own fade overrides.
- For preset playback, the incoming clip's fade sets projectM's `projectm_set_soft_cut_duration` before switching to the incoming preset. A fade of 0 seconds triggers a hard cut.
- The existing **Instant transitions** toggle overrides fades when enabled.
- Fades here refer to projectM preset soft transitions. They do **not** yet implement individual alpha fades for sprite overlays.
- All changes remain session-only.
- The separate **Export** tab still exports the currently loaded preset library as `.m3u8`, **not** the sequence as a show project or a video.
- Sprite playback requires integration with PR #2.

Manual QA: verify layout at 1080p and high DPI, per-preset fade lengths, zero-second hard cuts, sequence ordering, and simultaneous sprite staging.
