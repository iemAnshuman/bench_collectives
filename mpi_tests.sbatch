#!/bin/bash
#SBATCH --job-name=mpi_collectives       # Job name
#SBATCH --nodes=16                       # Number of nodes
#SBATCH --ntasks-per-node=16             # Tasks (ranks) per node
#SBATCH --cpus-per-task=1                # 1 core per task
#SBATCH --time=72:00:00                  # Time limit (hh:mm:ss)
#SBATCH --partition=buran                # Specify partition
#SBATCH --output=logs/output_%j.log      # Standard output log
#SBATCH --error=logs/error_%j.log        # Error log file
set -uo pipefail

module load gcc/14.2.0
module load openmpi/5.0.5

executable="$(pwd)/build/mpi/bin/benchmark_collectives_mpi"
pairs=("broadcast 1" "reduce 1" "scatter 1" "gather 1")
export PMIX_MCA_gds=hash
partition=buran
iterations=10
loop=10

execute_task() {
  local task_name=$1
  local count=$2

  # OpenMPI's tuned component names broadcast as "bcast"; map it so the
  # OMPI_MCA_coll_tuned_<op>_algorithm parameter actually takes effect.
  local mca_op=$task_name
  if [[ $task_name == "broadcast" ]]; then
    mca_op="bcast"
  fi

  for ((i=0; i<count; i++)); do
    for rpn in 16; do
        for node in 1 2 4 8 16; do
            # Zero-pad the node range, e.g. buran[00-15] (handles node > 10).
            local nodelist="${partition}[$(printf '%02d' 0)-$(printf '%02d' $((node - 1)))]"
            for test_size in 1 4 16 64 256 1024 4096 16384 65536 262144 1048576 4194304 16777216; do
                for (( j=0; j<loop; j++ )); do
                    srun --mpi=pmix --time=0:10:00 \
                        --export=ALL,OMPI_MCA_coll_tuned_use_dynamic_rules=1,OMPI_MCA_coll_tuned_${mca_op}_algorithm=$i \
                        --nodelist="$nodelist" -N "$node" --ntasks-per-node "$rpn" -c 1 \
                        "$executable" --algorithm="$i" --rpn="$rpn" --operation="$task_name" \
                        --test_size="$test_size" --iterations="$iterations"
                done
            done
        done
    done
  done
}

for pair in "${pairs[@]}"; do
  # Intentional word splitting: "<task> <count>" into positional parameters.
  # shellcheck disable=SC2086
  set -- $pair
  execute_task "$1" "$2"
done
