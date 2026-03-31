#!/usr/bin/env bash
set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJ/workspace/build/src/cef-terminal" && ./cef-frontend --no-sandbox
