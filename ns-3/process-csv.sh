#!/usr/bin/env bash

set +euf -o pipefail

for logpath in $1/*.log; do
    if [ ! -f "$logpath" ]; then
        echo "$logpath does not exist."
        continue
    fi

    csvpath="${logpath%.log}.csv"
    if [ -f "$csvpath" ]; then
        echo "$csvpath does exist, skipping re-computation."
        continue
    fi

    echo "Computing $csvpath"

    CLASSPATH=eval/old/src/ java statistics.Statistics "$logpath" > "$csvpath"
done
