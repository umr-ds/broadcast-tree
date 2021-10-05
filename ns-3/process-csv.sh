#!/usr/bin/env bash

set +euf -o pipefail

for logpath in $1/*-run.log; do
    if [ ! -f "$logpath" ]; then
        echo "$logpath does not exist."
        continue
    fi

    csvpath="${logpath%-run.log}.csv"
    if [ -f "$csvpath" ]; then
        echo "$csvpath does exist, skipping re-computation."
        continue
    fi

    echo "Computing $csvpath"

    CLASSPATH=eval/old/src/ java statistics.Statistics "$logpath" > "$csvpath"
done
