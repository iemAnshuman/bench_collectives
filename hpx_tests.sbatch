#!/bin/bash
#SBATCH --job-name=hpx_collectives          # Job name
#SBATCH --nodes=4                 # Request up to 16 nodes
#SBATCH --ntasks-per-node=32        # Default: 16 tasks per node
#SBATCH --cpus-per-task=2           # 1 cores per task
#SBATCH --time=12:00:00             # Time limit (hh:mm:ss)
#SBATCH --partition=workq         # Specify partition
#SBATCH --output=logs/output_%j.log      # Standard output log
#SBATCH --error=logs/error_%j.log        # Error log file
set -uo pipefail

#module load gcc/14.2.0
spack load gcc@14.3.0
parcelport=mpi # one of: tcp, mpi, lci
executable="$(pwd)/build/hpx/bin/benchmark_collectives_test"
iterations=10
loop=10

# Larger sizes (16384 65536 262144 1048576) omitted to fit the time limit.
for test_size in 1 2 4 8 16 64 256 1024 4096; do
    for lpn in 32; do
        for node in 1 2 4; do
            for arity in -1 2 4 8; do
                for operation in "broadcast" "reduce" "gather" "scatter"; do
                    echo "operation $operation, node $node, arity $arity, test_size $test_size, lpn $lpn"
                    for (( j=0; j<loop; j++ )); do
                        srun --mpi=pmi2 -N "$node" --ntasks-per-node "$lpn" -c 2 \
                             "$executable" --arity="$arity" --lpn="$lpn" --operation="$operation" \
                             --test_size="$test_size" --iterations="$iterations" \
                             --hpx:ini=hpx.parcel."$parcelport".enable=1 \
                             --hpx:ini=hpx.parcel.bootstrap="$parcelport" \
                             --hpx:ini=hpx.parcel."$parcelport".priority=1000
                    done
                done
            done
        done
    done
done
