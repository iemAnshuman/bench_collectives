set -e
ROOT=$(pwd)
#spack load /ezc6ffn #openmpi
module load gcc/14.2.0
spack install cxxopts
spack install openmpi@5.0.5%gcc@14.2.0 arch=linux-almalinux9-skylake_avx512
spack load openmpi@5.0.5%gcc@14.2.0 arch=linux-almalinux9-skylake_avx512
#module load openmpi/5.0.5
#spack load cxxopts
BUILD_DIR_MPI=${ROOT}/build/mpi/bin
mkdir -p ${BUILD_DIR_MPI}
cd ${BUILD_DIR_MPI}
mpicxx ${ROOT}/mpi/mpi_benchmark.cpp -I $(spack location -i cxxopts)/include -o benchmark_collectives_mpi

# TODO add hpx
#spack load gcc@14.2.0
module load gcc/14.2.0
#spack env activate dev_env/.
