#!/usr/bin/env bash

set -ex

PARCELPORT=lci #mpi #tcp
TCP=OFF
MPI=OFF
LCI=ON
# Load compiler and hpx for all dependencies
module load gcc/14.2.0
spack load hpx@1.11.0%gcc@14.2.0 arch=linux-almalinux9-skylake_avx512 malloc=jemalloc networking=tcp #${PARCELPORT}
# Configuration
export CC=gcc
export CXX=g++

BUILD_TYPE=Release
CMAKE_COMMAND=cmake
HPX_VERSION=master

ROOT=$(pwd)
DIR_SRC=${ROOT}/hpx
DIR_BUILD=${ROOT}/hpx/build_${PARCELPORT}
DIR_INSTALL=${ROOT}/install

DOWNLOAD_URL="git@github.com:STEllAR-GROUP/hpx.git"
#DOWNLOAD_URL="https://github.com/STEllAR-GROUP/hpx.git"

# Clone HPX if not present
if [[ ! -d ${DIR_SRC} ]]; then
    (
      mkdir -p ${DIR_SRC}
      cd ${DIR_SRC}
      git clone ${DOWNLOAD_URL} .
      git checkout ${HPX_VERSION}
    )
fi
# Create build directory if not present
if [[ ! -d ${DIR_BUILD} ]]; then
    (
      mkdir -p ${DIR_BUILD}
    )
fi
# Configure with cmake if not present
if [[ ! -d ${DIR_BUILD}/CMakeFiles ]]; then
    (
        ${CMAKE_COMMAND} \
            -H${DIR_SRC} \
            -B${DIR_BUILD} \
            -DCMAKE_INSTALL_PREFIX=${DIR_INSTALL} \
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
            -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
            -DCMAKE_EXE_LINKER_FLAGS="${LDCXXFLAGS}" \
            -DCMAKE_SHARED_LINKER_FLAGS="${LDCXXFLAGS}" \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
            -DHPX_WITH_MALLOC=jemalloc \
            -DHPX_WITH_NETWORKING=ON \
            -DHPX_WITH_DISTRIBUTED_RUNTIME=ON \
            -DHPX_WITH_MORE_THAN_64_THREADS=ON \
            -DHPX_WITH_MAX_CPU_COUNT=256 \
            -DHPX_WITH_LOGGING=OFF \
            -DHPX_WITH_EXAMPLES=OFF \
            -DHPX_WITH_TESTS=ON \
            -DHPX_WITH_APEX=OFF \
            -DHPX_WITH_PKGCONFIG=OFF \
            -DHPX_WITH_PARCELPORT_TCP=${TCP} \
            -DHPX_WITH_PARCELPORT_MPI=${MPI} \
            -DHPX_WITH_PARCELPORT_LCI=${LCI} \
            -DHPX_WITH_FETCH_LCI=${LCI}
    )
fi

# Compile
${CMAKE_COMMAND} --build ${DIR_BUILD} --target tests.performance.modules.collectives.benchmark_collectives -- -j
