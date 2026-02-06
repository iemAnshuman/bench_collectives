set -e
# Load dependencies
module load gcc/14.2.0
module load openmpi/5.0.5
spack install cxxopts

# Compile
ROOT=$(pwd)
BUILD_DIR_MPI=${ROOT}/build/mpi/bin
mkdir -p ${BUILD_DIR_MPI}
cd ${BUILD_DIR_MPI}
mpicxx ${ROOT}/mpi/mpi_benchmark.cpp -I $(spack location -i cxxopts)/include -o benchmark_collectives_test
