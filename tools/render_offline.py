#!/usr/bin/env python3
"""Encode a ThisIs395 render-job using projectMSDL-offline and FFmpeg."""
import argparse
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys


def load_job(path):
    job = json.loads(path.read_text(encoding="utf-8"))
    if job.get("schema") != "thisis395.render-job.v1":
        raise ValueError("Unsupported render manifest version")
    out = job["output"]
    width, height, fps = (int(out[x]) for x in ("width", "height", "fps"))
    if (not 16 <= width <= 7680 or not 16 <= height <= 4320 or
            width % 2 or height % 2 or not 1 <= fps <= 240):
        raise ValueError("Invalid dimensions or FPS (width/height must be even)")
    codec = out.get("codec", "h264")
    if codec not in ("h264", "hevc", "prores_422"):
        raise ValueError("Unsupported codec")
    dest = Path(out["path"]).expanduser()
    if not dest.is_absolute():
        dest = path.parent / dest
    if dest.exists() or not dest.parent.is_dir():
        raise ValueError("Output already exists or destination directory is missing")
    if job["tracks"].get("sprites"):
        raise ValueError("Sprites cannot be rendered yet; remove sprite clips")
    presets = []
    for item in job["tracks"].get("presets", []):
        src = Path(item["path"]).expanduser()
        if not src.is_absolute():
            src = path.parent / src
        if not src.is_file():
            raise ValueError("Missing preset: " + str(src))
        start = float(item["startSeconds"])
        length = float(item["durationSeconds"])
        fade = float(item.get("fadeSeconds", 0.0))
        if not all(math.isfinite(x) for x in (start, length, fade)) or start < 0 or length <= 0 or fade < 0:
            raise ValueError("Invalid clip timing")
        presets.append((start, length, fade, str(src)))
    if not presets:
        raise ValueError("At least one preset is required")
    presets.sort()
    if presets[0][0] != 0:
        raise ValueError("First preset must start at 0")
    for prior, next_clip in zip(presets, presets[1:]):
        if abs(next_clip[0] - (prior[0] + prior[1])) > 0.001:
            raise ValueError("Preset tracks must be contiguous (no gaps or overlaps)")
    length = presets[-1][0] + presets[-1][1]
    audio = job.get("audioTrack") or {}
    audio_src = audio.get("path", "") or job.get("audioSource", "")
    if audio_src:
        audio_src = Path(audio_src).expanduser()
        if not audio_src.is_absolute():
            audio_src = path.parent / audio_src
        if not audio_src.is_file():
            raise ValueError("Missing audio: " + str(audio_src))
        audio_start = float(audio.get("startSeconds", 0))
        audio_length = float(audio.get("durationSeconds", length))
        gain = float(audio.get("gain", 1))
        if not all(math.isfinite(x) for x in (audio_start, audio_length, gain)) or audio_start < 0 or audio_length <= 0 or not 0 <= gain <= 2:
            raise ValueError("Invalid audio track parameters")
        length = max(length, audio_start + audio_length)
    else:
        audio_start, audio_length, gain = 0., 0., 1.
        audio_src = None
    if length > 36000:
        raise ValueError("Timeline exceeds 10 hours")
    bitrate = int(out.get("bitrateMbps", 18))
    if not 1 <= bitrate <= 500:
        raise ValueError("Invalid bitrate")
    return locals()


def render(job, backend, ffmpeg):
    cmd = [backend, "--width", str(job["width"]), "--height", str(job["height"]),
           "--fps", str(job["fps"]), "--duration", f'{job["length"]:.6f}']
    for start, duration, fade, path in job["presets"]:
        cmd += ["--preset", f"{start:.6f}|{duration:.6f}|{fade:.6f}|{path}"]
    encode = [ffmpeg, "-hide_banner", "-loglevel", "warning", "-nostdin", "-n",
              "-f", "rawvideo", "-pixel_format", "rgb24",
              "-video_size", f'{job["width"]}x{job["height"]}',
              "-framerate", str(job["fps"]), "-i", "pipe:0"]
    if job["audio_src"]:
        encode += ["-i", str(job["audio_src"]), "-filter_complex",
                   f"[1:a]atrim=0:{job['audio_length']:.6f},asetpts=PTS-STARTPTS,"
                   f"volume={job['gain']:.6f},adelay={round(job['audio_start']*1000)}:all=1[a]",
                   "-map", "0:v:0", "-map", "[a]", "-c:a", "aac", "-b:a", "192k"]
    else:
        encode += ["-map", "0:v:0", "-an"]
    codec = job["codec"]
    if codec == "h264":
        encode += ["-c:v", "libx264", "-pix_fmt", "yuv420p",
                   "-b:v", str(job["bitrate"]) + "M"]
    elif codec == "hevc":
        encode += ["-c:v", "libx265", "-pix_fmt", "yuv420p",
                   "-b:v", str(job["bitrate"]) + "M", "-tag:v", "hvc1"]
    else:
        encode += ["-c:v", "prores_ks", "-profile:v", "2", "-pix_fmt", "yuv422p10le"]
    encode += ["-vf", "vflip", "-t", f'{job["length"]:.6f}', str(job["dest"])]

    producer = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    try:
        consumer = subprocess.Popen(encode, stdin=subprocess.PIPE)
        try:
            while True:
                data = producer.stdout.read(1048576)
                if not data:
                    break
                consumer.stdin.write(data)
            consumer.stdin.close()
            producer.stdout.close()
            a, b = producer.wait(), consumer.wait()
            if a or b:
                raise RuntimeError(f"Native projectM returned {a}, FFmpeg returned {b}")
        except BaseException:
            consumer.kill()
            consumer.wait()
            raise
    except BaseException:
        producer.kill()
        producer.wait()
        if job["dest"].exists():
            job["dest"].unlink()
        raise
    print("Rendered:", job["dest"])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("manifest", type=Path)
    p.add_argument("--backend", default="projectMSDL-offline")
    p.add_argument("--ffmpeg", default="ffmpeg")
    args = p.parse_args()
    try:
        job = load_job(args.manifest.expanduser().resolve())
        for binary in (args.backend, args.ffmpeg):
            if not shutil.which(binary):
                raise ValueError("Executable not found: " + binary)
        render(job, args.backend, args.ffmpeg)
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as exc:
        print("Offline render error:", exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
