# Qt6 native control center: feature branch

This branch adds an optional Qt6 Widgets control center beside the existing SDL/ImGui application.

## Native functionality implemented in source

- Visualizer launch/stop in a separate process, native file/folder dialogs, log output, and persisted launch preferences.
- **Show Editor** with **Playlist**, **Sequence**, **Render**, and **Export** tabs.
- Import .milk presets and PNG sprite assets into independent libraries; insert selected entries into a shared sequence.
- Reorder/remove sequence clips; edit their durations and incoming fades.
- Save and reopen editor projects with schema \`thisis395.native-show.v1\`. This is separate from the renderer manifest format.
- Configure video width, height, frame rate, codec, bitrate, audio file, audio length/gain, and target path.
- Export \`thisis395.render-job.v1\` JSON for the existing \`tools/render_offline.py\` offline renderer. Clip offsets are contiguous per lane, matching existing renderer assumptions.
- One independent **options pop-out shell** (currently informational).

## Limitations

The Qt editor currently maintains its own show state. It does **not** yet synchronize live playback, sequence cues or Sprite Manager changes to the SDL process. The old SDL ImGui editors remain fully available. Render-job JSON export does not itself run FFmpeg. The original rendering backend requires separate setup and testing. Native show projects are not importable by the existing ImGui editor.

The native app is **opt-in**, so existing SDL builds do not require Qt:

\`\`\`sh
cmake -S . -B cmake-build -DENABLE_QT_NATIVE_UI=ON
cmake --build cmake-build --config Release
\`\`\`

Qt 6 Widgets development libraries are required. Linux's standalone native UI compilation workflow is in \`.github/workflows/native-ui-check.yml\`. A successful compile is not a physical GPU/runtime test, and does not prove platform packaging.

### Native Windows/macOS/Linux distribution

Qt runtime platform plugins must be packaged. On macOS, locate the SDL program binary inside the .app and handle application bundle launching. Windows needs Qt DLL and \`platforms/qwindows\` deployment. macOS Universal builds and Linux desktop integration remain to be validated.

## Next integration work

1. Define a project/show model shared by Qt and the live renderer. Decide how to reconcile live ImGui edits, show loading and external changes.
2. Add IPC for preset cue, playback status, seek, stop, sequence lock/next/previous, sprite scene changes and errors, with a protocol/version handshake.
3. Convert the current informational options pop-out into complete editable native settings.
4. Add native render invocation/progress/cancel with checked prerequisites and safe output handling.
5. Extend Qt CI to Windows and macOS and test full packaging plus real projectM rendering before merging this branch.
