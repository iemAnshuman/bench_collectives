#!/bin/bash
#SBATCH --job-name=broadcast_tests           # Job name
#SBATCH --nodes=16                 # Request up to 16 nodes
#SBATCH --ntasks-per-node=16        # Default: 16 tasks per node
#SBATCH --cpus-per-task=2           # 2 cores per task
#SBATCH --time=12:00:00             # Time limit (hh:mm:ss)
#SBATCH --partition=medusa         # Specify partition
#SBATCH --output=output_%j.log      # Standard output log
#SBATCH --error=error_%j.log        # Error log file

for test_size in 1 2 4 8 16 64 256 1024 4096 16384 65536 262144 1048576; do
    for lpn in 1 16; do
        for node in 1; do
            for arity in -1 2 4 8 16; do 
                for operation in "broadcast" "reduce" "gather" "scatter"; do
                    echo "operation $operation, node $node, arity $arity, test_size $test_size, lpn $lpn"
                    srun -N $node --ntasks-per-node $lpn -c 2 build/my_hpx_program --arity=$arity --lpn=$lpn --operation=$operation --test_size=$test_size
                done
            done
        done
    done
done
