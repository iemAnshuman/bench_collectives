set -e
spack load /ezc6ffn #openmpi
spack load cxxopts
mpicxx mpi_benchmark.cpp -I $(spack location -i cxxopts)/include -o test_mpi

spack load gcc@14.2.0
spack env activate dev_env/.
spack install
rm -rf build
mkdir build
cd build
cmake .. -DHPX_IGNORE_BOOST_COMPATIBILITY=ON
make
#mv test_hpx ../test_hpx
