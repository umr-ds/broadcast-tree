#!/usr/bin/env bash

# Links files from Auswertungen.zip to experiement specific locations

target="results/thesis/"
mkdir -p "$target"

btp="true"
hops="0"

for log in auswertungen/*/normal*/*/output_*.log; do
    IFS="/" read -ra params <<< "$log"

    nodes=${params[3]}
    size=`echo ${params[1]} | sed s/x/_/`
    seed=`grep "SIMULATION SEED:" $log | head -n1 | cut -d' ' -f3`
    cpm=`echo ${params[4]} | cut -d_ -f2 | cut -d. -f1`

    rts_cts="false"
    if [[ ${params[2]} == *"_rc" ]]; then
        rts_cts="true"
    fi

    name="${nodes}_${hops}_${btp}_${size}_${seed}_${cpm}_${rts_cts}"
    path="${target}/${name}.log"

    echo Nodes: $nodes Hops: $hops BTP: $btp Size: $size Seed: $seed CPM: $cpm RTS/CTS: $rts_cts
    ln -s "${PWD}/${log}" "${path}"
done
