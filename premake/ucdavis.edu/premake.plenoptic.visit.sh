#!/bin/bash
/home/qadwu/Work/projects/ospray/sources/master/premake.sh \
    -o /home/qadwu/Work/projects/ospray/builds/ospray-visit-build \
    -i /home/qadwu/Work/projects/ospray/builds/ospray-visit-install \
    --mpi \
    --embree-dir "/home/qadwu/Work/softwares/embree/embree-3.2.0-install" \
    --tbb-dir "/home/qadwu/Work/softwares/tbb/tbb-2018_20180618-install" \
    --ispc-dir "/home/qadwu/Work/softwares/ispc/ispc-1.9.2-install" \
    --icc-dir "" \
    --gcc-dir "/usr/bin" \
    --cmake-dir "cmake" \
    -a " -D Snappy_DIR=/home/qadwu/Work/softwares/snappy/snappy-1.1.7-install/lib/cmake/Snappy " \
    -a " -D OSPRAY_BUILD_ISA=ALL" \
    -a " -D OSPRAY_MODULE_MPI=ON" \
    -a " -D OSPRAY_MODULE_MPI_APPS=OFF" \
    -a " -D OSPRAY_SG_CHOMBO=OFF" \
    -a " -D OSPRAY_SG_VTK=OFF" \
    -a " -D OSPRAY_SG_OPENIMAGEIO=OFF" \
    -a " -D OSPRAY_APPS_EXAMPLEVIEWER=OFF" \
    -a " -D OSPRAY_APPS_BENCHMARK=OFF" \
    -a " -D OSPRAY_MODULE_VISIT=ON"

