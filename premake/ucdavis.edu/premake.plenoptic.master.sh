#!/bin/bash
/home/qadwu/Work/projects/ospray/sources/master/premake.sh \
    -o /home/qadwu/Work/projects/ospray/builds/ospray-master-build \
    -i /home/qadwu/Work/projects/ospray/builds/ospray-master-install \
    --mpi \
    -a " -DTFN_MODULE_ROOT=~/Work/projects/ospray/apps/TransferFunctionModule" \
    -a " -DOSPRAY_MODULE_BRICKTREE=ON" \
    -a " -DOSPRAY_MODULE_BRICKTREE_BENCH=ON" \
    -a " -DOSPRAY_MODULE_BRICKTREE_WIDGET=ON" \
    --embree-dir "/home/qadwu/Work/softwares/embree/embree-3.2.0-install" \
    --tbb-dir "/home/qadwu/Work/softwares/tbb/tbb-2018_20180618-install" \
    --ispc-dir "/home/qadwu/Work/softwares/ispc/ispc-1.9.2-install" \
    --icc-dir "" \
    --gcc-dir "/usr/bin" \
    --cmake-dir "cmake"
