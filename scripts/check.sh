#!/usr/bin/env bash
# Everything CommandDispatcher can check on its own: the native unit tests, and the
# example compiled for each board it claims to support.
#
#   scripts/check.sh            from anywhere inside the repo
#   PIO=/path/to/pio scripts/check.sh

set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

# pio is often not on PATH when installed by the VS Code extension.
PIO="${PIO:-$(command -v pio || echo "$HOME/.platformio/penv/bin/pio")}"

step() { printf '\n=== %s ===\n' "$1"; }

step "Native tests"
"$PIO" test -e native

step "Example: Console (uno, esp32)"
"$PIO" ci examples/Console/Console.ino --lib "$root/src" \
    --board uno --board esp32dev

step "All green"
