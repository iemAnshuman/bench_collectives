#!/bin/bash
#SBATCH --job-name=broadcast_tests           # Job name
#SBATCH --nodes=16                 # Request up to 16 nodes
#SBATCH --ntasks-per-node=16        # Default: 16 tasks per node
#SBATCH --cpus-per-task=2           # 2 cores per task
#SBATCH --time=12:00:00             # Time limit (hh:mm:ss)
#SBATCH --partition=medusa         # Specify partition
#SBATCH --output=output_%j.log      # Standard output log
#SBATCH --error=error_%j.log        # Error log file

##pairs=("bcast 7" "gather 3" "reduce 8" "scatter 4")
pairs=("reduce 1" "scatter 1" "gather 1" )
export PMIX_MCA_gds=hash

execute_task() {
  local task_name=$1
  local count=$2
  for ((i=0; i<count; i++)); do
    for lpn in 1 2 4 8; do
        for node in 1; do
            for test_size in 1 4 16 64 256 1024 4096 16384 65536 262144 1048576 4194304 16777216 67108864; do
                srun --wait=0 --mpi=pmix --export=ALL,OMPI_MCA_coll_tuned_use_dynamic_rules=1,OMPI_MCA_coll_tuned_${task_name}_algorith=$i -N $node --ntasks-per-node $lpn -c 2 test_mpi --algorithm=$i --lpn=$lpn --operation=$task_name --test_size=$test_size
            done
        done
    done
    # Call your actual function here
  done
  for ((i=0; i<count; i++)); do
    for lpn in 16; do
        for node in 1 2 4 8 16; do
            for test_size in 4194304 16777216 67108864; do
                srun --wait=0 --mpi=pmix --export=ALL,OMPI_MCA_coll_tuned_use_dynamic_rules=1,OMPI_MCA_coll_tuned_${task_name}_algorith=$i -N $node --ntasks-per-node $lpn -c 2 test_mpi --algorithm=$i --lpn=$lpn --operation=$task_name --test_size=$test_size
            done
        done
    done
    # Call your actual function here
  done
}

for pair in "${pairs[@]}"; do
  set -- $pair  # Splitting into positional parameters
  execute_task "$1" "$2"
done
