# One-click render prerequisites (experimental)

The Render tab now provides **Install / repair render prerequisites** and **Render video with FFmpeg**.

- The setup action is **user-triggered** and invokes the operating-system package manager.
- Windows uses winget for `Gyan.FFmpeg` and `Python.Python.3.12`.
- macOS uses existing Homebrew for `python` and `ffmpeg`. Homebrew itself is not installed silently.
- Linux uses apt, dnf or pacman where available. sudo may require authentication.
- Restart the application after installing dependencies so the new PATH is available.
- The projectMSDL-offline executable is installed in app packages where projectM 4.2+ supports it.
- The Python render driver and prerequisite setup script are packaged alongside the app.
- The UI launches the render script as a separate process and does **not** report completion/progress yet.
- The existing render-job JSON is regenerated before launching and refuses overwrite. Choose new job and output paths for each export.
- FFmpeg itself is not bundled. Downloads and software licenses are governed by package providers.
- Offline visualizer FFT reaction and sprite rendering remain unsupported. Hardware smoke testing is required.

Known caveat: GUI installations may not show the package manager's terminal/stdout prompts depending on platform; run the packaged setup script from a terminal if the process cannot complete.
