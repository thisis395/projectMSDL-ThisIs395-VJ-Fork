#!/bin/sh
# ThisIs395 VJ user-invoked dependency installer. No silent auto-install.
set -eu
echo "ThisIs395 VJ: installing Python 3 and FFmpeg"
if [ "$(uname -s)" = "Darwin" ]; then
  if ! command -v brew >/dev/null 2>&1; then
    echo "Homebrew is required: https://brew.sh" >&2
    exit 1
  fi
  brew install python ffmpeg
elif command -v apt-get >/dev/null 2>&1; then
  sudo apt-get update
  sudo apt-get install -y python3 ffmpeg
elif command -v dnf >/dev/null 2>&1; then
  sudo dnf install -y python3 ffmpeg
elif command -v pacman >/dev/null 2>&1; then
  sudo pacman -S --needed python ffmpeg
else
  echo "Unsupported package manager. Install python3 and ffmpeg from your distro." >&2
  exit 1
fi
echo "Setup complete. Restart ThisIs395 VJ to refresh its environment."
