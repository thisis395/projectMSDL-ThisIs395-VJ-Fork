# PR #4 refinement: separate Playlist / Sequence, transport and audio track

- **Playlist** and **Sequence** are distinct top-level tabs. Playlist retains library browsing, stars, and + to append clips to Sequence.
- Sequence has **Previous**, **Play sequence/Restart**, **Next**, **Stop**, and **Lock preset** controls. Previous/Next cue the preceding/following preset clip and skip sprite-only entries. Lock prevents automatic advancement but allows manual navigation. Unlock resumes timing from the current preset.
- The existing Presets and Sprites sub-tabs remain under Sequence with editable clip durations and preset fades.
- Render now shows a **third audio lane** alongside Presets and Sprites. A source file path, start time, duration, and gain are editable and serialized into `audioTrack` in the JSON render-job manifest.
- The audio lane depicts intended placement only. It does **not** decode or play a media file, generate a waveform, synchronize FFT, or export audio/video yet.
- All three timeline tracks are visual and clip-based; there is no drag-and-drop, actual trim, or independent offscreen program monitor.
- Validation: test separate tabs, + insertion, lock/unlock and previous/next behavior, timeline audio bar and JSON serialization; build on all OS platforms.
