#!/usr/bin/env bash
set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
WS="$PROJ/workspace"
(
  cd "$WS/build"
  cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
  ninja cef-terminal
)
echo "[build] Binary at: $WS/build/src/Release/cef-terminal"
