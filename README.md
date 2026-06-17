# Benchmark collectives

Compile HPX with three parcelports (TCP, MPI, LCI) and benchmark its new
hierarchical collectives, alongside an MPI reference benchmark for comparison.

Both benchmarks support `broadcast`, `reduce`, `scatter`, `gather`,
`all_gather`, `all_reduce`, and `all_to_all`, validate correctness on every
iteration, and append per-run statistics to a `result/` CSV file
(semicolon-separated, one header per file).

## Repository layout

| Path | Purpose |
| --- | --- |
| `compile_hpx.sh` | Clone and build HPX + the collectives benchmark target. |
| `compile_mpi.sh` | Build the MPI reference benchmark via CMake. |
| `mpi/mpi_benchmark.cpp` | MPI reference benchmark source. |
| `mpi/CMakeLists.txt` | CMake build for the MPI benchmark. |
| `run.sh` | Submit all HPX + MPI job combinations (parcelports × node counts) in one shot. |
| `hpx_tests.sbatch` | SLURM batch sweep for HPX (parcelport and node count as CLI args). |
| `mpi_tests.sbatch` | SLURM batch sweep for MPI (node count as CLI arg). |
| `logs/`, `result/` | Job logs and benchmark output (git-ignored). |

## Requirements

- `gcc/14.2.0` and `openmpi/5.0.5` on rostam/medusa (loaded via `module load`)
- `gcc@14.3.0` and `openmpi@5.0.10` on qbd (loaded via `spack load`)
- CMake ≥ 3.16
- [`cxxopts`](https://github.com/jarro2783/cxxopts) — used by the MPI benchmark.
  Found via `find_package` if installed, otherwise fetched automatically during
  the CMake configure step.

## How to compile

HPX benchmark: `./compile_hpx.sh`
→ produces `build/hpx/bin/benchmark_collectives_test`

MPI benchmark: `./compile_mpi.sh`
→ produces `build/mpi/bin/benchmark_collectives_mpi`

## How to run

### All jobs at once

```
./run.sh
```

Submits HPX (tcp, mpi, lci) × MPI reference for node counts 1, 2, 4 — one
sbatch job per combination. Logs land in `logs/output_<jobid>.log` and
`logs/error_<jobid>.log`.

### Manually

HPX benchmark (the MPI parcelport requires a negative priority):

```
module load gcc/14.2.0
srun -p medusa -N 2 --ntasks-per-node 2 -c 1 \
    build/hpx/bin/benchmark_collectives_test \
    --arity=2 --lpn=2 --operation=reduce --test_size=1024 --iterations=10 \
    --hpx:ini=hpx.parcel.tcp.enable=1 \
    --hpx:ini=hpx.parcel.bootstrap=tcp \
    --hpx:ini=hpx.parcel.tcp.priority=1000
```

MPI benchmark:

```
srun --mpi=pmix --export=ALL,OMPI_MCA_coll_tuned_use_dynamic_rules=1,OMPI_MCA_coll_tuned_reduce_algorithm=1 \
    -p medusa -N 2 --ntasks-per-node 2 -c 1 \
    build/mpi/bin/benchmark_collectives_mpi \
    --algorithm=1 --rpn=2 --operation=reduce --test_size=1024 --iterations=10
```

Note: OpenMPI's tuned component uses `bcast`/`allgather`/`allreduce`/`alltoall`
as the MCA parameter suffix even though the benchmark uses `broadcast`/`all_gather`
etc. as operation names.

### Sbatch scripts

Both scripts accept CLI arguments to override defaults:

```
sbatch --nodes=4 hpx_tests.sbatch --parcelport=mpi --nodes=4
sbatch --nodes=4 mpi_tests.sbatch --nodes=4
```

The `--nodes=` flag must be passed both to `sbatch` (for resource allocation)
and as a script argument (so the benchmark knows the topology). `run.sh` handles
this automatically.

## Output

Results are written to:

- HPX: `result/hpx/<parcelport>/<num_localities>/<collective>/runtimes_<collective>_hpx.txt`
- MPI: `result/mpi/<num_ranks>/<collective>/runtimes_<collective>_mpi.txt`

Both formats use the same semicolon-separated columns:

```
collective;module;algorithm;nodes;ranks;rpn;size;warmup;iterations;mean;variance;stddev;min;max;median
```

Timing records the **slowest rank's latency** per iteration (via `MPI_MAX`
reduction) so all collectives are measured on the same basis and results are
directly comparable.

## Contact

alexander.strack@ipvs.uni-stuttgart.de
