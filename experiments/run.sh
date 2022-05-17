#! /usr/bin/env bash

CONFIG_PATH="debug.conf"

for POLL_TIMEOUT in $(seq 100 400 1100); do
    for DISCOVERY_BCAST_INTERVAL in $(seq 100 400 1100); do
        for PENDING_TIMEOUT in $(seq 100 400 1100); do
            for SOURCE_RETRANSMIT_PAYLOAD in $(seq 100 400 1100); do
                for UNCHANGED_COUNTER in $(seq 5 5 25); do
                    for OMIT_ROLL_BACK in true false; do
                        echo "Running configuration $POLL_TIMEOUT, $DISCOVERY_BCAST_INTERVAL, $PENDING_TIMEOUT, $SOURCE_RETRANSMIT_PAYLOAD, $UNCHANGED_COUNTER, $OMIT_ROLL_BACK"
                        echo "[CLIENTS]
id = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 70, 71, 72, 73, 75, 76, 77, 78, 79, 80, 83 ]

[SOURCE]
id = 0

[EXPERIMENT]
payload_size = \"1K\"
max_power = false
iterations = 1

poll_timeout = $POLL_TIMEOUT
discovery_bcast_interval = $DISCOVERY_BCAST_INTERVAL
pending_timeout = $PENDING_TIMEOUT
source_retransmit_payload = $SOURCE_RETRANSMIT_PAYLOAD

unchanged_counter = $UNCHANGED_COUNTER
omit_roll_back = $OMIT_ROLL_BACK" > "$CONFIG_PATH"

                        ./run.py -c "$CONFIG_PATH"
                    done
                done
            done
        done
    done
done

