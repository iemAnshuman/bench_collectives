spack load /ezc6ffn #openmpi
spack load cxxopts
mpicxx mpi_benchmark.cpp -I $(spack location -i cxxopts)/include -o test_mpi

spack load hpx@1.10.0
spack load gcc@14.2.0
mkdir build
cd build
cmake ..
make
mv test_hpx ../test_hpx
