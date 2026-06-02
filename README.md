# Benchmark collectives

Compile HPX with three parcelports (TCP, MPI, LCI) and benchmark its new
hierarchical collectives, alongside an MPI reference benchmark for comparison.

Both benchmarks support `broadcast`, `reduce`, `scatter`, and `gather`, validate
correctness on every iteration, and append the mean/variance of the measured
runtimes to a `result/` text file (semicolon-separated, one header per file).

## Repository layout

| Path | Purpose |
| --- | --- |
| `compile_hpx.sh` | Clone and build HPX + the collectives benchmark target. |
| `compile_mpi.sh` | Build the MPI reference benchmark via CMake. |
| `mpi/mpi_benchmark.cpp` | MPI reference benchmark source. |
| `mpi/CMakeLists.txt` | CMake build for the MPI benchmark. |
| `hpx_tests.sbatch`, `mpi_tests.sbatch` | SLURM batch sweeps. |
| `hpx_tests.sh` | Interactive HPX sweep (direct `srun`, no sbatch). |
| `logs/`, `result/` | Job logs and benchmark output (git-ignored). |

## Requirements

- `gcc/14.2.0` and `openmpi/5.0.5` (loaded via `module load`)
- CMake ≥ 3.16
- [`cxxopts`](https://github.com/jarro2783/cxxopts) — used by the MPI benchmark.
  It is found via `find_package` if installed, otherwise fetched automatically
  during the CMake configure step.

## How to compile

HPX benchmark: `./compile_hpx.sh`
→ produces `build/hpx/bin/benchmark_collectives_test`

MPI benchmark: `./compile_mpi.sh`
→ produces `build/mpi/bin/benchmark_collectives_mpi`

## How to run

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

Note: OpenMPI's tuned component names broadcast as `bcast`, so the MCA parameter
is `OMPI_MCA_coll_tuned_bcast_algorithm` even though the benchmark uses
`--operation=broadcast`.

### Sbatch scripts

HPX benchmark: `sbatch hpx_tests.sbatch`

MPI benchmark: `sbatch mpi_tests.sbatch`

## Output

Results are written to `result/<runtime>/<collective>/runtimes_<collective>_<runtime>.txt`
with columns:

```
collective;module;algorithm;nodes;ranks;rpn;size;iterations;mean;variance
```

## Contact

alexander.strack@ipvs.uni-stuttgart.de
