#!/bin/bash

set -e

if [[ ! -d deps ]]; then
    echo "The directory deps does not exist."
    echo "Please run ./get_deps.sh and try again."
    false
fi

source deps/env.sh

if [[ $(uname -s) = Linux ]]; then
    DEFAULT_THREADS=$(nproc --all)
elif [[ $(uname -s) = Darwin ]]; then
    DEFAULT_THREADS=$(sysctl -n hw.ncpu)
else
    DEFAULT_THREADS=4
fi
THREADS=${THREADS:-$DEFAULT_THREADS}

make -C core clean
make -C core -j$THREADS

if [[ $TASKBENCH_USE_MPI -eq 1 ]]; then
    make -C mpi clean
    make -C mpi all -j$THREADS
fi

if [[ $USE_GASNET -eq 1 ]]; then
    make -C "$GASNET_DIR"
fi

if [[ $USE_HWLOC_TMP -eq 1 ]]; then
    pushd "$HWLOC_SRC_DIR"
    ./configure --prefix=$HWLOC_DIR
    make -j$THREADS
    make install
    popd
fi

if [[ $USE_LEGION -eq 1 ]]; then
    make -C legion clean
    make -C legion -j$THREADS
fi

if [[ $USE_STARPU -eq 1 ]]; then
    STARPU_CONFIGURE_FLAG="--disable-cuda --disable-opencl --disable-fortran --disable-build-tests --disable-build-examples "
    if [[ $USE_HWLOC_TMP -eq 1 ]]; then
      STARPU_CONFIGURE_FLAG+="" 
    else
      STARPU_CONFIGURE_FLAG+="--without-hwloc"
    fi 
    pushd "$STARPU_SRC_DIR"
    PKG_CONFIG_PATH=$HWLOC_DIR/lib/pkgconfig ./configure --prefix=$STARPU_DIR $STARPU_CONFIGURE_FLAG
    make -j$THREADS
    make install
    popd
    make -C starpu clean
    make -C starpu -j$THREADS
fi

if [[ $USE_PARSEC -eq 1 ]]; then
    mkdir -p "$PARSEC_DIR"
    pushd "$PARSEC_DIR"
    if [[ $USE_HWLOC_TMP -eq 1 ]]; then
      ../contrib/platforms/config.linux -DPARSEC_GPU_WITH_CUDA=OFF -DCMAKE_INSTALL_PREFIX=$PWD -DHWLOC_DIR=$HWLOC_DIR
    else
      ../contrib/platforms/config.linux -DPARSEC_GPU_WITH_CUDA=OFF -DCMAKE_INSTALL_PREFIX=$PWD
    fi
    make -j$THREADS
    make install
    popd
    make -C parsec clean
    make -C parsec -j$THREADS
fi

(if [[ $USE_CHARM -eq 1 ]]; then
    if [[ -n $CRAYPE_VERSION ]]; then
        module load craype-hugepages8M
    fi
    pushd "$CHARM_DIR"
    ./build charm++ $CHARM_VERSION --with-production -j$THREADS
    popd
    pushd "$CHARM_SMP_DIR"
    ./build charm++ $CHARM_VERSION smp --with-production -j$THREADS
    popd
    make -C charm++ clean
    make -C charm++
    (
        export CHARM_DIR="$CHARM_SMP_DIR"
        rm -rf charm++_smp
        cp -r charm++ charm++_smp
        make -C charm++_smp clean
        make -C charm++_smp
     )
fi)
  
if [[ $USE_OPENMP -eq 1 ]]; then
    make -C openmp clean
    make -C openmp -j$THREADS
fi

if [[ $USE_OMPSS -eq 1 ]]; then
    #mkdir -p "$LIBUNWIND_BUILD"
    #pushd "$LIBUNWIND_SRC_DIR"
    #./configure prefix=$LIBUNWIND_BUILD
    #make -j$THREADS
    #make install prefix=$LIBUNWIND_BUILD
    #popd
    #mkdir -p "$EXTRAE_BUILD"
    #pushd "$EXTRAE_SRC_DIR"
    #./configure --prefix=$EXTRAE_BUILD --with-unwind=$LIBUNWIND_BUILD --without-dyninst --without-papi --without-mpi
    #make -j$THREADS
    #make install    
    #popd
    mkdir -p "$NANOS_BUILD"
    pushd "$NANOS_SRC_DIR"
    ./configure --prefix=$NANOS_BUILD --disable-instrumentation --disable-debug 
    make -j$THREADS
    make install
    popd
    mkdir -p "$MERCURIUM_BUILD"
    pushd "$MERCURIUM_SRC_DIR"
    ./configure --prefix=$MERCURIUM_BUILD --enable-ompss --with-nanox=$NANOS_BUILD --with-hwloc=$HWLOC_DIR
    make -j$THREADS
    make install
    popd
    #source deps/env.sh
    make -C ompss clean
    make -C core -j$THREADS
    make -C ompss -j$THREADS
fi
