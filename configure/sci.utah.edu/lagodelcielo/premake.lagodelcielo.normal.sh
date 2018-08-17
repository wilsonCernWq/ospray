#!/bin/bash
PIDX_DIR=/home/sci/qwu/software/Lagodelcielo/PIDX/install
TURBOJPEG_DIR=/home/sci/qwu/software/Lagodelcielo/libjpeg-turbo/install
TFN_MODULE_ROOT=/home/sci/qwu/OSPRay/apps/TransferFunctionModule
/home/sci/qwu/OSPRay/source/master/premake.sh  \
    --mpi --gcc \
    -o /ssd/users/qwu/ospray/build \
    -i /ssd/users/qwu/ospray/install \
    --gcc-exec "gcc" "g++" \
    --tbb-dir "/opt/intel/compilers_and_libraries_2018.1.163/linux/tbb" \
    --embree-dir "/home/sci/qwu/software/embree-3.2.0.x86_64.linux" \
    --ispc-dir "/home/sci/qwu/software/ispc-v1.9.1-linux" \
    --icc-dir "" \
    --gcc-dir "/usr/bin" \
    --cmake-dir "cmake" \
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
    -a " -D OSPRAY_MODULE_TUBES=OFF" 

