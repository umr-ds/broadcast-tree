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
                    for cpm in "${CPM[@]}"; do
                        for size in "${SIZE[@]}"; do
                            for rts_cts in "${RTS_CTS[@]}"; do
                                name="${nodes}_${hops}_${btp}_${size}_${size}_${seed}_${runs}_${cpm}_${rts_cts}"

                                echo "# Starting $name"
                                cmd="./waf --run-no-build=\"broadcast
                                    --nWifi=${nodes}
                                    --hopCount=${hops}
                                    --eebtp=${btp}
                                    --width=${size}
                                    --height=${size}
                                    --rndSeed=${seed}
                                    --iMax=${runs}
                                    --cpm=${cpm}
                                    --rtsCts=${rts_cts}
                                    --linearEnergyModel=false
                                    --log=true\" > \"$log_path/$name.err\" 2>\"$log_path/$name.log\""

                                sem -j+0 bash -c "'$cmd'"

                                echo "# `ps aux | grep "python3 ./waf" | wc -l` parallel jobs running"
                            done
                        done
                    done
                done
            done
        done
    done

    while true; do echo "# Waiting for `ps aux | grep "python3 ./waf" | wc -l` jobs."; sleep 5; done &
    statuspid="$!"

    sem --wait
    kill "$statuspid"
    echo "Done."

else
    echo "# Dropping into bash"
    bash
fi
