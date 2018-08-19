#!/bin/bash
TURBOJPEG_DIR=/home/sci/qwu/software/FSM/libjpeg-turbo/install
PIDX_DIR=/home/sci/qwu/software/FSM/PIDX/install
TFN_MODULE_ROOT=/home/sci/qwu/OSPRay/apps/TransferFunctionModule
/home/sci/qwu/OSPRay/source/master/premake.sh  \
    --mpi \
    -o /ssd/qwu/ospray/build \
    -i /ssd/qwu/ospray/install \
    --gcc \
    --embree-dir "/home/sci/qwu/software/embree-3.2.0.x86_64.linux" \
    --tbb-dir "/home/sci/qwu/software/tbb2017_20170604oss" \
    --ispc-dir "/home/sci/qwu/software/ispc-v1.9.1-linux" \
    --icc-dir "" --gcc-dir "/opt/rh/devtoolset-7/root/usr/bin" \
    --cmake-dir "cmake3" \
    -a " -D PIDX_DIR=${PIDX_DIR}" \
    -a " -D TURBOJPEG_DIR=${TURBOJPEG_DIR}" \
    -a " -D TFN_MODULE_ROOT=${TFN_MODULE_ROOT}" \
    -a " -D OSPRAY_BUILD_ISA=ALL" \
    -a " -D OSPRAY_MODULE_MPI=ON" \
    -a " -D OSPRAY_MODULE_MPI_APPS=ON" \
    -a " -D OSPRAY_SG_CHOMBO=ON" \
    -a " -D OSPRAY_SG_VTK=OFF" \
    -a " -D OSPRAY_SG_OPENIMAGEIO=ON" \
    -a " -D OSPRAY_APPS_EXAMPLEVIEWER=ON" \
    -a " -D OSPRAY_APPS_BENCHMARK=ON" \
    -a " -D OSPRAY_MODULE_PIDX=ON" \
    -a " -D OSPRAY_MODULE_PIDX_WORKER=ON" \
    -a " -D OSPRAY_MODULE_PIDX_VIEWER=ON" \
    -a " -D OSPRAY_MODULE_COSMICWEB=ON" \
    -a " -D OSPRAY_MODULE_COSMICWEB_WORKER=ON" \
    -a " -D OSPRAY_MODULE_COSMICWEB_VIEWER=ON" \
    -a " -D OSPRAY_MODULE_BRICKTREE=ON" \
    -a " -D OSPRAY_MODULE_BRICKTREE_BENCH=ON" \
    -a " -D OSPRAY_MODULE_BRICKTREE_WIDGET=ON" \
    -a " -D OSPRAY_MODULE_IMPLICIT_ISOSURFACES=OFF" \
    -a " -D OSPRAY_MODULE_IMPI_VIEWER=OFF" \
    -a " -D OSPRAY_MODULE_IMPI_BENCH_WIDGET=OFF" \
    -a " -D OSPRAY_MODULE_IMPI_BENCH_MARKER=OFF" \
    -a " -D OSPRAY_MODULE_TFN=OFF" \
    -a " -D OSPRAY_MODULE_TFN_WIDGET=OFF" \
    -a " -D OSPRAY_MODULE_SPLATTER=OFF" \
    -a " -D OSPRAY_MODULE_TUBES=OFF" \
    -a " -D OSPRAY_MODULE_VISIT=OFF"
