#!/bin/sh
# Launched from the verified package after the user accepts a restart.
set -eu
old_pid=$1
app=$2
payload=$3
config=$4
log="$(dirname "$(dirname "$payload")")/update-error.txt"
backup="${app}.previous-${old_pid}"
candidate="${app}.updating-${old_pid}"

install_update() {
  attempts=0
  while kill -0 "$old_pid" 2>/dev/null; do
    attempts=$((attempts + 1))
    [ "$attempts" -lt 240 ] || { echo 'Editor did not exit within two minutes.'; return 1; }
    sleep .5
  done
  [ -d "$app" ] && [ -d "$payload" ] || { echo 'Application or update payload missing.'; return 1; }
  [ ! -e "$backup" ] && [ ! -e "$candidate" ] || { echo 'Backup or update candidate already exists.'; return 1; }
  /usr/bin/ditto "$payload" "$candidate"
  /usr/bin/codesign --verify --deep --strict "$candidate"
  /bin/mv "$app" "$backup"
  if ! /bin/mv "$candidate" "$app"; then
    /bin/mv "$backup" "$app" || true
    echo 'Could not install; restored previous application.'
    return 1
  fi
  if [ "${D4BOX_UPDATE_TEST_ONLY:-}" = 1 ]; then
    "$app/Contents/MacOS/D4BoxEditor" --smoke-test
  elif [ -n "$config" ]; then
    "$app/Contents/MacOS/D4BoxEditor" "$config" >/dev/null 2>&1 &
  else
    "$app/Contents/MacOS/D4BoxEditor" >/dev/null 2>&1 &
  fi
}

if ! install_update >"$log" 2>&1; then
  if [ "${D4BOX_UPDATE_TEST_ONLY:-}" != 1 ] && [ -x "$app/Contents/MacOS/D4BoxEditor" ]; then
    if [ -n "$config" ]; then
      "$app/Contents/MacOS/D4BoxEditor" "$config" >/dev/null 2>&1 &
    else
      "$app/Contents/MacOS/D4BoxEditor" >/dev/null 2>&1 &
    fi
  fi
  exit 1
fi
