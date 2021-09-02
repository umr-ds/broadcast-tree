#!/bin/bash

# Start CORE gui if $DISPLAY is set
if [ ! -z "$RUN" ]; then
    echo "# Starting experiments from path $RUN"
    source $RUN
    ./waf --run="broadcast
        --nWifi=${NODES}
        --hopCount=${HOP_COUNT}
        --eebtp=${EEBTP}
        --width=${SIZE}
        --height=${SIZE}
        --rndSeed=${SEED}
        --iMax=${RUNS}
        --cpm=${CPM}
        --tracing=true
        --rtsCts=true
        --linearEnergyModel=false" > ns3.log 2>run.log

else
    echo "# Dropping into bash"
    bash
fi
