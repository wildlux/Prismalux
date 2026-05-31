#!/usr/bin/env bash
# uninstall.sh — Rimuove Prismalux dal sistema Linux
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "$SCRIPT_DIR/install_launcher.sh" --uninstall "$@"
