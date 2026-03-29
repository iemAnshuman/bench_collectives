#!/bin/bash
#SBATCH --job-name=mpi_collectives           # Job name
#SBATCH --nodes=16                 # Request up to 16 nodes
#SBATCH --ntasks-per-node=16        # Default: 16 tasks per node
#SBATCH --cpus-per-task=1           # 2 cores per task
#SBATCH --time=72:00:00             # Time limit (hh:mm:ss)
#SBATCH --partition=buran         # Specify partition
#SBATCH --output=logs/output_%j.log      # Standard output log
#SBATCH --error=logs/error_%j.log        # Error log file

module load gcc/14.2.0
module load openmpi/5.0.5

executable=$(pwd)/build/mpi/bin/benchmark_collectives_mpi
pairs=("broadcast 1" "reduce 1" "scatter 1" "gather 1" )
export PMIX_MCA_gds=hash
partition=buran
iterations=10
loop=10

execute_task() {
  local task_name=$1
  local count=$2
  for ((i=0; i<count; i++)); do
    for rpn in 16; do
        for node in 1 2 4 8 16; do
            for test_size in 1 4 16 64 256 1024 4096 16384 65536 262144 1048576 4194304 16777216; do
                for (( j=0; j<$loop; j=j+1 ))
                do
                    srun --mpi=pmix --time=0:10:00 --export=ALL,OMPI_MCA_coll_tuned_use_dynamic_rules=1,OMPI_MCA_coll_tuned_${task_name}_algorith=$i \
                        --nodelist=$partition[00-0$(($node - 1))] -N $node --ntasks-per-node $rpn -c 1 \
                        $executable --algorithm=$i --rpn=$rpn --operation=$task_name --test_size=$test_size --iterations=$iterations
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
