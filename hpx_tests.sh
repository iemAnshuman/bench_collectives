#!/bin/bash
# Interactive HPX collectives sweep (runs srun directly, no sbatch).
# Do not use `set -e` here: a single failing srun should not abort the sweep.
set -uo pipefail

export LCI_ATTR_IBV_TD_STRATEGY=none

module load gcc/14.2.0
partition=buran
parcelport=lci
executable="$(pwd)/build/hpx/bin/benchmark_collectives_test"
iterations=10
loop=10

for lpn in 16; do
    for node in 1 2 4 8 16; do
        # Zero-pad the node range, e.g. buran[00-15] (handles node > 10 correctly).
        nodelist="${partition}[$(printf '%02d' 0)-$(printf '%02d' $((node - 1)))]"
        for test_size in 1 4 16 64 256 1024 4096 16384 65536 262144 1048576 4194304 16777216; do
            for arity in -1 2 4 8; do
                for operation in "broadcast" "reduce" "gather" "scatter"; do
                    echo "operation $operation, node $node, arity $arity, test_size $test_size, lpn $lpn"
                    for (( j=0; j<loop; j++ )); do
                        srun --mpi=pmix -p "$partition" --time=0:10:00 --nodelist="$nodelist" \
                            -N "$node" --ntasks-per-node "$lpn" -c 1 \
                            "$executable" --arity="$arity" --lpn="$lpn" --operation="$operation" \
                            --test_size="$test_size" --iterations="$iterations" \
                            --hpx:ini=hpx.parcel.mpi.enable=0 \
                            --hpx:ini=hpx.parcel.tcp.enable=0 \
                            --hpx:ini=hpx.parcel.lci.enable=0 \
                            --hpx:ini=hpx.parcel."$parcelport".enable=1
                    done
                done
            done
        done
    done
done
