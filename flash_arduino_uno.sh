#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH_DIR="$ROOT_DIR/arduino/uart_bridge_test"
FQBN="arduino:avr:uno"
PORT="${1:-}"

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "arduino-cli not found. Install with: brew install arduino-cli"
  exit 1
fi

if [[ -z "$PORT" ]]; then
  while IFS= read -r line; do
    candidate="${line%% *}"
    if [[ "$candidate" == /dev/cu.usbmodem* ]] || [[ "$candidate" == /dev/cu.usbserial* ]]; then
      PORT="$candidate"
      break
    fi
  done < <(arduino-cli board list | awk 'NR>1 && $1 ~ "^/dev/cu\\." {print $0}')
fi

if [[ -z "$PORT" ]]; then
  echo "No Arduino serial port found."
  echo "Plug the board in, then run: arduino-cli board list"
  exit 1
fi

echo "Using port: $PORT"
arduino-cli compile --fqbn "$FQBN" "$SKETCH_DIR"
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH_DIR"

echo "Upload complete."
echo "Monitor with: arduino-cli monitor -p $PORT -c baudrate=115200"
