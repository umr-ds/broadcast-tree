#! /usr/bin/env bash

NODES=(1 2)             # Number of devices
HOP_COUNT=(2 3)         # The max hop count a broadcast message in SBP can travel
EEBTP=(true)            # If EE-BTP or SBP should be used
SEED=1000               # Seed for deterministic randomnes (e.g., where nodes are placed)
RUNS=10                 # Number of simulations per parameter set
CPM=(1)                 # The cycle prevention method to use
SIZE=(100)              # Size of simulation area (size x size)
RTS_CTS=(true false)    # Whether to use RTS/CTS