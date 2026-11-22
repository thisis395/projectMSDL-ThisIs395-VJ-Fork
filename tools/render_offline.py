#!/usr/bin/env python3
"""Encode a ThisIs395 render-job using projectMSDL-offline and FFmpeg."""
import argparse
import json
import math
import os
import tempfile
import time
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
    sprites = []
    for item in job["tracks"].get("sprites", []):
        src = Path(item["path"]).expanduser()
        if not src.is_absolute():
            src = path.parent / src
        if not src.is_file() or src.suffix.lower() != ".png":
            raise ValueError("Missing or invalid sprite PNG: " + str(src))
        start, dur, fade = (float(item.get(k, 0)) for k in ("startSeconds", "durationSeconds", "fadeSeconds"))
        if not all(math.isfinite(v) for v in (start, dur, fade)) or start < 0 or dur <= 0 or fade < 0:
            raise ValueError("Invalid sprite timing")
        sprites.append((start, dur, fade, str(src)))
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
    if sprites:
        length = max(length, max(s[0] + s[1] for s in sprites))
    return locals()


def write_status(path, state, **fields):
    if path is None:
        return
    payload = {"state": state, **fields}
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(json.dumps(payload), encoding="utf-8")
    os.replace(temp, path)


def render(job, backend, ffmpeg, status=None, cancel=None):
    # Decode once to f32le stereo 48 kHz for projectM's FFT, using the same audio
    # track and trim parameters as the output mux. Never interpret arbitrary
    # source audio file bytes as PCM samples.
    pcm_path = None
    producer = None
    consumer = None
    total_frames = math.ceil(job["length"] * job["fps"] - 1e-9)
    expected_bytes = total_frames * job["width"] * job["height"] * 3
    transferred = 0
    try:
        cmd = [backend, "--width", str(job["width"]), "--height", str(job["height"]),
               "--fps", str(job["fps"]), "--duration", f'{job["length"]:.6f}']
        for start, duration, fade, source in job["presets"]:
            cmd += ["--preset", f"{start:.6f}|{duration:.6f}|{fade:.6f}|{source}"]
        for start, duration, fade, source in job["sprites"]:
            cmd += ["--sprite", f"{start:.6f}|{duration:.6f}|{fade:.6f}|{source}"]

        if job["audio_src"]:
            with tempfile.NamedTemporaryFile(prefix="thisis395-fft-", suffix=".f32", delete=False) as raw:
                pcm_path = Path(raw.name)
            decode = [ffmpeg, "-hide_banner", "-nostdin", "-loglevel", "error", "-y",
                      "-i", str(job["audio_src"]), "-t", f'{job["audio_length"]:.6f}',
                      "-vn", "-f", "f32le", "-acodec", "pcm_f32le", "-ar", "48000",
                      "-ac", "2", str(pcm_path)]
            write_status(status, "preparing_audio")
            subprocess.run(decode, check=True)
            cmd += ["--pcm", str(pcm_path), "--audio-start", str(job["audio_start"]),
                    "--audio-gain", str(job["gain"])]

        encode = [ffmpeg, "-hide_banner", "-loglevel", "warning", "-nostdin", "-n",
                  "-f", "rawvideo", "-pixel_format", "rgb24",
                  "-video_size", f'{job["width"]}x{job["height"]}',
                  "-framerate", str(job["fps"]), "-i", "pipe:0"]
        if job["audio_src"]:
            encode += ["-i", str(job["audio_src"]), "-filter_complex",
                       f"[1:a]atrim=0:{job['audio_length']:.6f},asetpts=PTS-STARTPTS,"
                       f"volume={job['gain']:.6f},adelay={round(job['audio_start'] * 1000)}:all=1[a]",
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

        write_status(status, "rendering", progress=0.0)
        producer = subprocess.Popen(cmd, stdout=subprocess.PIPE)
        consumer = subprocess.Popen(encode, stdin=subprocess.PIPE)
        last_report = 0
        assert producer.stdout and consumer.stdin
        while True:
            if cancel and cancel.exists():
                raise InterruptedError("Render cancelled")
            data = producer.stdout.read(1048576)
            if not data:
                break
            consumer.stdin.write(data)
            transferred += len(data)
            if time.monotonic() - last_report >= 0.5:
                write_status(status, "rendering", progress=min(1.0, transferred / expected_bytes))
                last_report = time.monotonic()
        consumer.stdin.close()
        producer.stdout.close()
        a = producer.wait()
        b = consumer.wait()
        if a or b or transferred != expected_bytes:
            raise RuntimeError(f"Export failed (projectM={a}, FFmpeg={b}, "
                               f"RGB bytes={transferred}/{expected_bytes})")
        if not job["dest"].is_file() or job["dest"].stat().st_size == 0:
            raise RuntimeError("FFmpeg finished without producing a nonempty output")
        write_status(status, "completed", progress=1.0, output=str(job["dest"]))
        print("Rendered:", job["dest"])
    except BaseException as exc:
        write_status(status, "cancelled" if isinstance(exc, InterruptedError) else "failed",
                     error=str(exc))
        for process in (consumer, producer):
            if process and process.poll() is None:
                process.kill()
            if process:
                process.wait()
        if job["dest"].exists():
            job["dest"].unlink()
        raise
    finally:
        if pcm_path is not None:
            pcm_path.unlink(missing_ok=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("manifest", type=Path)
    p.add_argument("--backend", default="projectMSDL-offline")
    p.add_argument("--ffmpeg", default="ffmpeg")
    p.add_argument("--status", type=Path, help="Machine-readable render status JSON")
    p.add_argument("--cancel", type=Path, help="Cancellation marker file")
    args = p.parse_args()
    try:
        if args.cancel and args.cancel.exists():
            args.cancel.unlink()
        write_status(args.status, "validating")
        job = load_job(args.manifest.expanduser().resolve())
        for binary in (args.backend, args.ffmpeg):
            if not shutil.which(binary):
                raise ValueError("Executable not found: " + binary)
        render(job, args.backend, args.ffmpeg, args.status, args.cancel)
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as exc:
        write_status(args.status, "failed", error=str(exc))
        print("Offline render error:", exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
