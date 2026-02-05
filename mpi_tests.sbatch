#!/bin/bash
#SBATCH --job-name=mpi_collectives           # Job name
#SBATCH --nodes=4                 # Request up to 16 nodes
#SBATCH --ntasks-per-node=16        # Default: 16 tasks per node
#SBATCH --cpus-per-task=1           # 2 cores per task
#SBATCH --time=12:00:00             # Time limit (hh:mm:ss)
#SBATCH --partition=medusa         # Specify partition
#SBATCH --output=logs/output_%j.log      # Standard output log
#SBATCH --error=logs/error_%j.log        # Error log file

executable=$(pwd)/build/mpi/bin/benchmark_collectives_mpi
pairs=("broadcast 1" "reduce 1" "scatter 1" "gather 1" )
export PMIX_MCA_gds=hash
iterations=10
loop=5

execute_task() {
  local task_name=$1
  local count=$2
  for ((i=0; i<count; i++)); do
    for rpn in 16; do
        for node in 1 2 4; do
            for test_size in 1 4 16 64 256 1024 4096; do
                #16384 65536 262144 1048576 4194304 16777216 67108864; do
                for (( j=0; j<$loop; j=j+1 ))
                do
                    srun --mpi=pmix --export=ALL,OMPI_MCA_coll_tuned_use_dynamic_rules=1,OMPI_MCA_coll_tuned_${task_name}_algorith=$i -N $node --ntasks-per-node $rpn -c 1 $executable --algorithm=$i --rpn=$rpn --operation=$task_name --test_size=$test_size --iterations=$iterations
                done
            done
        done
    done
  done
}

for pair in "${pairs[@]}"; do
  set -- $pair  # Splitting into positional parameters
  execute_task "$1" "$2"
done
