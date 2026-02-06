# Benchmark collectives

This repo allows to compile HPX with three parcelports and benchmark the new hierarchical collectives.
Additionally, a MPI reference benchmark is included.

## How to compile

HPX Benchmark: `./compile_hpx.sh`

MPI Benchmark: `./compile_mpi.sh` (requires cxxopts)

## How to run 

### Manually:

HPX Benchmark: (mpi requires negative priority)


```
module load gcc/14.2.0
srun -p medusa -N 2 --ntasks-per-node 2 -c 1 \
    build/hpx/bin/benchmark_collectives_test \
    --arity=2 --lpn=2 --operation=reduce --test_size=1024 --iterations=10 \
    --hpx:ini=hpx.parcel.tcp.enable=1 \
    --hpx:ini=hpx.parcel.bootstrap=tcp \
    --hpx:ini=hpx.parcel.tcp.priority=1000
```

MPI Benchmark:

```
srun --mpi=pmix --export=ALL,OMPI_MCA_coll_tuned_use_dynamic_rules=1,OMPI_MCA_coll_tuned_reduce_algorith=1 \
    -p medusa -N 2 --ntasks-per-node 2 -c 1 \
    build/mpi/bin/benchmark_collectives_test \
    --algorithm=1 --rpn=2 --operation=reduce --test_size=1024 --iterations=10
```

### Sbatch Scripts

HPX Benchmark: `sbatch hpx_tests.sbatch`

MPI Benchmark: `sbatch mpi_tests.sbatch`

## Contact

alexander.strack@ipvs.uni-stuttgart.de
