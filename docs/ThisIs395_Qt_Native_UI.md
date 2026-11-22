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


## First live-renderer control bridge (implemented in source, not yet validated)

The control center creates a unique private \`QTemporaryDir\`, passes it to the existing SDL executable with \`--controlDir\`, and sends a single in-flight JSON command using \`QSaveFile\` atomic replacement.

The SDL \`RenderLoop\` services \`command.json\` on the **render thread**, then publishes \`response.json\` with a version, matching ID, success/error, and current preset/shuffle/lock state. The UI polls for responses, reports errors, and times out after five seconds.

Supported v1 actions:

| Action | Meaning |
|---|---|
| \`ping\` | Initial protocol handshake |
| \`next\`, \`previous\` | Advance/reverse current preset |
| \`shuffle\` | Toggle live shuffle |
| \`lock\` | Toggle automatic preset change lock |
| \`cue\` (+ \`path\`) | Cue a specific preset, **only if already in the live projectM playlist** |

Example request:

\`\`\`json
{"version":1,"id":"example-unique-id","action":"next"}
\`\`\`

No TCP port is opened. This mailbox is a simple local prototype, not a production-grade multi-client IPC system. Command files are limited to 8 KB. Commands are serial and clients must match response IDs.

**Not supported yet:** sending the complete Qt sequence into the live renderer, controlling timed sprite scenes, reconciling editor-side and renderer-side mutations, remote control, or bidirectional event streaming. The existing ImGui editor and the Qt project file remain independent.

### Bridge QA before merging

1. Start SDL from the Qt control center. Verify the \`ping\` handshake and error display.
2. Trigger Next, Previous, Shuffle, Lock, and Cue while music is playing.
3. Confirm that a cue for a preset absent from the live playlist produces a user-facing error.
4. Kill the renderer; verify clean timeout and restart behavior.
5. Check build and runtime behavior on Windows, macOS, and Linux, particularly QTemporaryDir permissions and Poco file-renaming behavior.
6. Add integration tests and robust persistence/restart logic before retiring the old UI.


## Downloadable Qt6 standalone artifacts

`.github/workflows/native-ui-check.yml` now builds an independent Qt6 native executable
using `src/native/standalone/CMakeLists.txt` on Windows x64, macOS Intel and Linux x64.
Unlike the SDL build check, this workflow is intended to produce **Qt UI downloads**:

- `ThisIs395-VJ-NativeUI-windows-x64`: executable, required Qt DLLs, and the Windows platform plugin from `windeployqt`.
- `ThisIs395-VJ-NativeUI-macos-intel`: unsigned `.app` with Qt frameworks via `macdeployqt`.
- `ThisIs395-VJ-NativeUI-linux-x64`: AppImage using linuxdeploy and the Qt deployment plugin.

These are **Qt control-center artifacts**, not complete bundled SDL/projectM+FFmpeg workstations.
The Qt UI still requires a separately built or installed `projectMSDL`/`projectM`
visualizer binary to launch live rendering. On macOS the user may need to grant
Gatekeeper approval for unsigned development packages. GitHub Actions artifact
retention is set to 30 days.

The workflow has been committed; **do not claim download availability or successful
packaging until the corresponding GitHub Actions run completes**.


## Unified VJ suite build (Qt + SDL together)

The native-editor-only artifacts from `native-ui-check.yml` remain useful for Qt smoke tests, but they do **not** include the SDL renderer. The new `.github/workflows/unified-suite.yaml` builds **both targets in the same project** using `-DENABLE_QT_NATIVE_UI=ON`, and packages them as a suite.

| Platform | Unified artifact | Layout |
|---|---|---|
| Windows x64 | `ThisIs395-VJ-suite-windows-x64` | Single ZIP containing the Qt launcher, `projectMSDL.exe`, Qt DLLs/plugins, and installed renderer files |
| macOS Intel | `ThisIs395-VJ-suite-macos-intel` | One `projectM.app` with Qt control center as Finder entrypoint and `projectM` renderer in the same `Contents/MacOS` directory |
| Ubuntu Linux x64 | `ThisIs395-VJ-suite-linux-x64` | Single DEB installing both executables; Qt6 runtime comes from system package dependencies |

Two executables are intentional. Qt remains a separate process so the renderer has its own pop-out OpenGL window. **One build and one distribution does not mean one OS process.** When the integrated suite installs both executables together, the native control center's default renderer path resolves to the sibling executable.

Build manually with the full source dependencies and Qt6 Widgets:

```sh
cmake -S . -B build -DENABLE_QT_NATIVE_UI=ON
cmake --build build --config Release
cmake --install build --config Release --prefix /some/install/path
```

**CI status:** The unified packaging workflow must pass before these artifact names are considered downloadable. Previously built native-only UI artifacts do not retroactively become integrated packages. The Qt-to-renderer mailbox and separate-window arrangement still need physical smoke testing.
