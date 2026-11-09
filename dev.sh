#!/usr/bin/env bash
# Preserve the caller's working directory and arguments.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "$0")" && pwd)"
if ! command -v python3 >/dev/null 2>&1; then
  echo "需要 Python 3.9+，请先安装 Python。" >&2
  exit 2
fi
exec python3 "$ROOT/scripts/dev_editor.py" "$@"
