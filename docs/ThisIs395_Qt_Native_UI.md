# Qt6 native control center (first increment)

This feature is **opt-in** and does not replace the existing SDL/ImGui app.

## What works

- A Qt6 Widgets desktop window with OS-native controls and file/folder chooser dialogs.
- Configurable paths to the existing visualizer executable, preset folder, and texture folder.
- Width, height and fullscreen launch settings.
- Launch/stop the existing SDL visualizer in a separate OS window using QProcess.
- Display the child application's stdout/stderr and process exit state.
- Store configuration locally using platform-native QSettings.
- Pop out an independent options/info window.

## Build

Install Qt 6 Widgets development files alongside the existing project dependencies.

```sh
cmake -S . -B cmake-build -DENABLE_QT_NATIVE_UI=ON
cmake --build cmake-build --config Release
```

The executable is named `ThisIs395-VJ-ControlCenter` (with .exe on Windows).
Point it at a built `projectMSDL` / `projectM` binary if the renderer is not installed beside the control center.

## Explicit limitations

- This is the **first functioning native shell**, not a complete UI port.
- Playlist, Sequence, Render, Export, Sprite Manager and their settings are **still inside the old SDL/ImGui process**; native tab controls and shared project state must be implemented next.
- The pop-out options window currently contains setup information only. Full editable options in a pop-out are future work.
- A separate Qt binary is not yet integrated into the macOS .app layout or distribution packaging. Qt runtime deployment (including Windows platform plugins) requires testing.
- The existing renderer is a child process; closing the control center terminates it.
- No automated Qt-enabled CI job or physical cross-platform testing yet. Existing non-Qt CI is unaffected.

## Next phases

1. Extract a shared, testable show/project state model for playlist, sequence, sprites, render jobs, and configuration.
2. Implement real Qt Playlist, Sequence, Render and Export editors against that model.
3. Replace the placeholder options window with a detached, complete settings editor, and persist window layouts.
4. Add an IPC/control protocol between Qt and the renderer so controls work while visualizer runs independently.
5. Package Qt dependencies correctly for Windows, macOS Universal, and Linux; add Qt-enabled CI and manual rendering tests.
