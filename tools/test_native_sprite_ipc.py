#!/usr/bin/env python3
"""Headless integration smoke test for the real SDL renderer's Sprite Manager.

Run inside xvfb-run on Linux with a software OpenGL driver:
    xvfb-run -a python3 tools/test_native_sprite_ipc.py --exe ./ThisIs395-VJ
Unlike compile checks, this exercises PNG import, live scaling and sprite removal.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time
import uuid
import zlib


def png(path):
    # Tiny opaque green RGBA PNG; dependency-free to avoid test setup drift.
    width = height = 16
    raw = b"".join(b"\x00" + b"\x00\xf0\x00\xff" * width for _ in range(height))
    def chunk(tag, payload):
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(
            ">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def command(directory, process, action, **params):
    ident = uuid.uuid4().hex
    request = {"version": 1, "id": ident, "action": action, **params}
    temporary = directory / "command.tmp"
    temporary.write_text(json.dumps(request), encoding="utf-8")
    os.replace(temporary, directory / "command.json")
    deadline = time.monotonic() + 18
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"Renderer exited during {action}, code={process.returncode}")
        response_path = directory / "response.json"
        try:
            with response_path.open(encoding="utf-8") as stream:
                reply = json.load(stream)
        except (OSError, ValueError):
            time.sleep(0.1)
            continue
        try:
            response_path.unlink()
        except FileNotFoundError:
            pass
        if reply.get("id") != ident:
            continue
        if not reply.get("ok"):
            raise RuntimeError(f"Sprite IPC {action} failed: {reply.get('error')}")
        return reply
    raise TimeoutError(f"Sprite IPC {action} did not respond in 18 seconds")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    args = parser.parse_args()
    binary = args.exe.resolve()
    if not binary.is_file():
        raise FileNotFoundError(binary)
    with tempfile.TemporaryDirectory(prefix="thisis395-ipc-") as root:
        mailbox = Path(root)
        image = mailbox / "sprite_smoketest.png"
        png(image)
        env = os.environ.copy()
        env.setdefault("LIBGL_ALWAYS_SOFTWARE", "1")
        env.setdefault("SDL_AUDIODRIVER", "dummy")
        command_line = [
            str(binary), "--vj-renderer", "--controlDir", str(mailbox),
            "--width", "640", "--height", "360", "--fullscreen", "0",
            "--fps", "30", "--enableSplash", "1"
        ]
        with (mailbox / "renderer.log").open("wb") as logfile:
            process = subprocess.Popen(command_line, env=env,
                                       stdout=logfile, stderr=subprocess.STDOUT)
            try:
                result = command(mailbox, process, "ping")
                assert isinstance(result.get("sprites"), list), result
                result = command(mailbox, process, "sprite.add", path=str(image))
                assert len(result["sprites"]) == 1, result
                result = command(
                    mailbox, process, "sprite.update",
                    index=0, x=0.3, y=0.7, scale=0.6, opacity=0.8,
                    rotationRadians=0.4, gain=0.5, audioBand=1)
                assert abs(result["sprites"][0]["scale"] - 0.6) < 0.00001, result
                result = command(mailbox, process, "sprite.list")
                assert len(result["sprites"]) == 1, result
                result = command(mailbox, process, "sprite.remove", index=0)
                assert not result["sprites"], result
                print("PASS: ping, PNG import, live scale change, list and remove")
            except Exception:
                print("Renderer output tail:", flush=True)
                print((mailbox / "renderer.log").read_text(errors="replace")[-12000:])
                raise
            finally:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)


if __name__ == "__main__":
    main()
