#!/bin/bash
TURBOJPEG_DIR=/home/sci/qwu/software/Kepler/system/libjpeg-turbo/install
PIDX_DIR=/home/sci/qwu/software/Kepler/PIDX/install
TFN_MODULE_ROOT=/home/sci/qwu/OSPRay/apps/TransferFunctionModule
/home/sci/qwu/OSPRay/source/master/premake.sh \
    -o /home/sci/qwu/OSPRay/builds/Kepler/build \
    -i /home/sci/qwu/OSPRay/builds/Kepler/install \
    --mpi \
    --gcc \
    --embree-dir /home/sci/qwu/software/embree-3.2.0.x86_64.linux \
    --tbb-dir /home/sci/qwu/software/tbb2017_20170604oss \
    --ispc-dir /home/sci/qwu/software/ispc-v1.9.1-linux \
    --gcc-dir /home/sci/qwu/software/gcc/gcc-7.1.0-install/bin \
    --icc-dir /opt/intel/compilers_and_libraries_2017.2.174/linux/bin/intel64 \
    --cmake-dir cmake \
    -a " -DTURBOJPEG_DIR=${TURBOJPEG_DIR}" \
    -a " -DPIDX_DIR=${PIDX_DIR}" \
    -a " -DTFN_MODULE_ROOT=${TFN_MODULE_ROOT}" \
    -a " -D OSPRAY_BUILD_ISA=ALL" \
    -a " -D OSPRAY_MODULE_MPI=ON" \
    -a " -D OSPRAY_MODULE_MPI_APPS=ON" \
    -a " -D OSPRAY_SG_CHOMBO=OFF" \
    -a " -D OSPRAY_SG_VTK=OFF" \
    -a " -D OSPRAY_SG_OPENIMAGEIO=OFF" \
    -a " -D OSPRAY_APPS_EXAMPLEVIEWER=ON" \
    -a " -D OSPRAY_APPS_BENCHMARK=ON" \
    -a " -D OSPRAY_MODULE_PIDX=OFF" \
    -a " -D OSPRAY_MODULE_PIDX_WORKER=OFF" \
    -a " -D OSPRAY_MODULE_PIDX_VIEWER=OFF" \
    -a " -D OSPRAY_MODULE_COSMICWEB=ON" \
    -a " -D OSPRAY_MODULE_COSMICWEB_WORKER=ON" \
    -a " -D OSPRAY_MODULE_COSMICWEB_VIEWER=ON" \
    -a " -D OSPRAY_MODULE_BRICKTREE=OFF" \
    -a " -D OSPRAY_MODULE_BRICKTREE_BENCH=OFF" \
    -a " -D OSPRAY_MODULE_BRICKTREE_WIDGET=OFF" \
    -a " -D OSPRAY_MODULE_IMPLICIT_ISOSURFACES=OFF" \
    -a " -D OSPRAY_MODULE_IMPI_VIEWER=OFF" \
    -a " -D OSPRAY_MODULE_IMPI_BENCH_WIDGET=OFF" \
    -a " -D OSPRAY_MODULE_IMPI_BENCH_MARKER=OFF" \
    -a " -D OSPRAY_MODULE_TFN=OFF" \
    -a " -D OSPRAY_MODULE_TFN_WIDGET=OFF" \
    -a " -D OSPRAY_MODULE_SPLATTER=OFF" \
    -a " -D OSPRAY_MODULE_TUBES=OFF" \
    -a " -D OSPRAY_MODULE_VISIT=OFF"
