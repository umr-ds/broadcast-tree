NODES=(1 2 3 4 5)       # Number of devices
HOP_COUNT=(1 2 3 4 5)   # The max hop count a broadcast message in SBP can travel
EEBTP=(true flase)      # If EE-BTP or SBP should be used
SEED=1000               # Seed for deterministic randomnes (e.g., where nodes are placed)
RUNS=30                 # Number of simulations per parameter set
CPM=(0 1 2)             # The cycle prevention method to use
SIZE=(100 200 300 500)  # Size of simulation area (size x size)
