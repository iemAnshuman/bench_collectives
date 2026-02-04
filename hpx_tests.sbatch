#!/bin/bash
#SBATCH --job-name=hpx_collectives          # Job name
#SBATCH --nodes=4                 # Request up to 16 nodes
#SBATCH --ntasks-per-node=16        # Default: 16 tasks per node
#SBATCH --cpus-per-task=1           # 1 cores per task
#SBATCH --time=12:00:00             # Time limit (hh:mm:ss)
#SBATCH --partition=medusa         # Specify partition
#SBATCH --output=output_%j.log      # Standard output log
#SBATCH --error=error_%j.log        # Error log file

iterations=10
loop=5
for test_size in 1 2 4 8 16 64 256 1024 4096; do
    #16384 65536 262144 1048576; do
    for lpn in 16; do
        for node in 1 2 4; do
            for arity in -1 2 4 8; do 
                for operation in "broadcast" "reduce" "gather" "scatter"; do
                    echo "operation $operation, node $node, arity $arity, test_size $test_size, lpn $lpn"
                    for (( j=0; j<$loop; j=j+1 ))
                    do
                        srun -N $node --ntasks-per-node $lpn -c 1 test_hpx --arity=$arity --lpn=$lpn --operation=$operation --test_size=$test_size --iterations=$iterations
                    done
                done
            done
        done
    done
done
