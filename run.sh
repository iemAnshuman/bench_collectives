#!/usr/bin/env bash
# Submit HPX (all three parcelports) and MPI reference benchmark jobs for
# node counts 1, 2, 4 (powers of 2).  Each combination becomes one sbatch job.
# Usage: ./run.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for nodes in 1 2 4; do
    for pp in mpi tcp lci; do
        echo "Submitting HPX parcelport=$pp nodes=$nodes"
        sbatch --nodes="$nodes" \
               --job-name="hpx_${pp}_n${nodes}" \
               "${ROOT}/hpx_tests.sbatch" \
               --parcelport="$pp" --nodes="$nodes"
    done

    echo "Submitting MPI reference nodes=$nodes"
    sbatch --nodes="$nodes" \
           --job-name="mpi_ref_n${nodes}" \
           "${ROOT}/mpi_tests.sbatch" \
           --nodes="$nodes"
done
