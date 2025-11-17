spack load openmpi
spack load cxxopts
mpicxx mpi_benchmark.cpp -I $(spack location -i cxxopts)/include -o test
