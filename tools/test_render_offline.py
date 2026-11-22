#!/usr/bin/env python3
"""FFmpeg pipeline smoke test using deterministic synthetic RGB frames.

The synthetic producer tests CLI plumbing, muxing and status reporting without
claiming to exercise an actual SDL/OpenGL projectM context.
"""
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="vj-smoke-") as td:
        folder = Path(td)
        preset = folder / "smoke.milk"
        preset.write_text("[preset00]\n", encoding="utf-8")
        sprite = folder / "sprite.png"
        sprite.write_bytes(b"\x89PNG\r\n\x1a\n")
        backend = folder / "fake_projectm.py"
        backend.write_text(
            "#!/usr/bin/env python3\n"
            "import argparse, sys, math\n"
            "p=argparse.ArgumentParser()\n"
            "p.add_argument('--width',type=int,required=True)\n"
            "p.add_argument('--height',type=int,required=True)\n"
            "p.add_argument('--fps',type=int,required=True)\n"
            "p.add_argument('--duration',type=float,required=True)\n"
            "p.add_argument('--preset',action='append')\n"
            "p.add_argument('--sprite',action='append')\n"
            "p.add_argument('--pcm')\n"
            "p.add_argument('--audio-start')\n"
            "p.add_argument('--audio-gain')\n"
            "args=p.parse_args()\n"
            "for i in range(math.ceil(args.duration*args.fps-1e-9)):\n"
            "  rgb=bytes((i*15%256,30,180))*(args.width*args.height)\n"
            "  sys.stdout.buffer.write(rgb)\n",
            encoding="utf-8",
        )
        backend.chmod(0o755)
        audio = folder / "tone.wav"
        subprocess.run(
            ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi",
             "-i", "sine=frequency=220:duration=0.4", "-ac", "2", str(audio)],
            check=True,
        )
        output = folder / "test.mp4"
        job = {
            "schema": "thisis395.render-job.v1",
            "output": {"path": str(output), "width": 16, "height": 16, "fps": 10,
                       "codec": "h264", "bitrateMbps": 1},
            "tracks": {
                "presets": [{"path": str(preset), "startSeconds": 0,
                             "durationSeconds": 0.4, "fadeSeconds": 0}],
                "sprites": [{"path": str(sprite), "startSeconds": 0,
                             "durationSeconds": 0.4, "fadeSeconds": 0}],
            },
            "audioTrack": {"path": str(audio), "startSeconds": 0,
                           "durationSeconds": 0.4, "gain": 0.5},
        }
        manifest = folder / "test.render.json"
        manifest.write_text(json.dumps(job), encoding="utf-8")
        status = folder / "status.json"
        subprocess.run(
            [sys.executable, str(root / "tools" / "render_offline.py"),
             str(manifest), "--backend", str(backend), "--status", str(status)],
            check=True,
        )
        assert output.stat().st_size > 100
        result = json.loads(status.read_text(encoding="utf-8"))
        assert result["state"] == "completed", result
        media = subprocess.check_output(
            ["ffprobe", "-v", "error", "-show_entries", "stream=codec_type",
             "-of", "json", str(output)], text=True)
        kinds = {stream["codec_type"] for stream in json.loads(media)["streams"]}
        assert kinds == {"video", "audio"}, kinds
        print("Synthetic FFmpeg smoke test passed (video + audio + status).")


if __name__ == "__main__":
    main()
