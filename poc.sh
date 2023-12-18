#!/bin/bash

filepath="$1"
cmd="${@:2}"

if test -z "$filepath"; then
  echo "Usage: $0 path command args..." >&2
  exit 1
fi

echo "${cmd[@]}"
inotifywait --quiet --monitor --recursive $filepath -e modify |
  while read events; do
    # clear
    ${cmd[@]}
  done
