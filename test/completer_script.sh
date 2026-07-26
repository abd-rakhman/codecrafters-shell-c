#!/bin/bash
# argv[1] = command  argv[2] = word being completed  argv[3] = word before it
cmd="$1"
cur="$2"
prev="$3"

candidates=()

if [[ "$cmd" == "git" && "$prev" == "remote" ]]; then
  candidates=(add set-url remove rename show prune update)
elif [[ "$cmd" == "git" && "$prev" == "git" ]]; then
  candidates=(remote status commit push pull checkout branch)
fi

for c in "${candidates[@]}"; do
  if [[ "$c" == "$cur"* ]]; then
    echo "$c"
  fi
done
