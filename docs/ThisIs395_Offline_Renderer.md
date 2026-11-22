Offline renderer + FFmpeg integration (experimental)

This PR builds on the unmerged NLE PR #4, and should be merged after it.

Build: CMake builds projectMSDL-offline if projectM 4.2+ is present, desktop OpenGL is enabled, and ENABLE_OFFLINE_RENDERER=ON.
Requires working SDL2 OpenGL context even though its window is hidden. FFmpeg must be installed separately.

Render command:

    python3 tools/render_offline.py /path/to/show.render.json --backend /path/to/projectMSDL-offline

The native backend draws projectM at exact frame_index/FPS times, captures raw RGB frames and pipes them to FFmpeg.
FFmpeg vertically flips frames and encodes H.264, HEVC or ProRes, optionally attaching an audio file using the NLE track start, duration and gain.
Existing output files are not overwritten. The script rejects absent presets, malformed timing, and sprite clips it cannot render.

LIMITATIONS: Sprite compositing is not yet implemented. Muxed music is not supplied to projectM's FFT, so visuals are not audio-reactive for offline jobs yet.
There is no GUI render button, progress/cancel workflow, audio waveform, GPU-less execution, or true independent FBO implementation.
OpenGL readback is synchronous. This code has not yet been tested on physical GPU hardware.
Relative source paths resolve against the manifest's parent directory. Timeline preset clips must be contiguous.
