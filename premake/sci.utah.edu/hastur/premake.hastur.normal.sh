#!/bin/bash
TURBOJPEG_DIR=/home/sci/qwu/software/Hastur/libjpeg-turbo/install
PIDX_DIR=/home/sci/qwu/software/Hastur/PIDX/install
TFN_DIR=/home/sci/qwu/OSPRay/apps/TransferFunctionModule
VTK_DIR=~/software/Hastur/VTK/VTK-6.3.0-install/lib/cmake/vtk-6.3
/home/sci/qwu/OSPRay/source/master/premake.sh --mpi --gcc \
    -o /ssd/qwu/ospray/build \
    -i /ssd/qwu/ospray/install \
   --embree-dir /home/sci/qwu/software/embree-3.2.0.x86_64.linux \
    --tbb-dir /home/sci/qwu/software/tbb2017_20170604oss \
    --ispc-dir /home/sci/qwu/software/ispc-v1.9.1-linux \
    --icc-dir /opt/intel/compilers_and_libraries_2017.2.174/linux/bin/intel64 \
    --gcc-dir /home/sci/qwu/software/gcc/gcc-7.1.0-install/bin \
    --cmake-dir ~/software/cmake/cmake-3.11.0/bin/cmake \
    -a " -D TURBOJPEG_DIR=${TURBOJPEG_DIR}" \
    -a " -D PIDX_DIR=${PIDX_DIR}" \
    -a " -D VTK_DIR=${VTK_DIR}" \
    -a " -D TFN_MODULE_ROOT=${TFN_DIR}" \
    -a " -D OSPRAY_BUILD_ISA=ALL" \
    -a " -D OSPRAY_MODULE_MPI=ON" \
    -a " -D OSPRAY_MODULE_MPI_APPS=ON" \
    -a " -D OSPRAY_SG_CHOMBO=ON" \
    -a " -D OSPRAY_SG_VTK=ON" \
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
