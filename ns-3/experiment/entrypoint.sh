#!/bin/bash

# Start CORE gui if $DISPLAY is set
if [ -n "$RUN" ]; then
    echo "# Starting experiments from path $RUN"
    source "$RUN"

    exp_folder=$(date +"%Y-%m-%d_%H%M%S")
    log_path="/results/$exp_folder"

    mkdir -p "$log_path"

    for nodes in "${NODES[@]}"; do
        for hops in "${HOP_COUNT[@]}"; do
            for btp in "${EEBTP[@]}"; do
                for seed in "${SEED[@]}"; do
                    for runs in "${RUNS[@]}"; do
                        for cpm in "${CPM[@]}"; do
                            for size in "${SIZE[@]}"; do
                                for rts_cts in "${RTS_CTS[@]}";do
                                    log_name="${nodes}_${hops}_${btp}_${size}_${size}_${seed}_${runs}_${cpm}_${rts_cts}-run.log"
                                    err_name="${nodes}_${hops}_${btp}_${size}_${size}_${seed}_${runs}_${cpm}_${rts_cts}-err.log"
                                    echo "# Executing $log_name"
                                    ./waf --run="broadcast
                                        --nWifi=${nodes}
                                        --hopCount=${hops}
                                        --eebtp=${btp}
                                        --width=${size}
                                        --height=${size}
                                        --rndSeed=${seed}
                                        --iMax=${runs}
                                        --cpm=${cpm}
                                        --rtsCts=${rts_cts}
                                        --linearEnergyModel=false" > "$log_path/$err_name" 2>"$log_path/$log_name"
                                    sed -i "1 i\Nodes: $nodes, Hops: $hops, Proto: $btp, Size: $size, Seed: $seed, Runs: $runs, CPM: $cpm, RTS: $rts_cts" "$log_path/$log_name"
                                done
                            done
                        done
                    done
                done
            done
        done
    done

else
    echo "# Dropping into bash"
    bash
fi
