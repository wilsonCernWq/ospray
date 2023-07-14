#!/bin/bash
## Copyright 2014 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

#### Helper functions ####

function check_symbols
{
  for sym in `nm $1 | grep $2_`
  do
    version=(`echo $sym | sed 's/.*@@\(.*\)$/\1/g' | grep -E -o "[0-9]+"`)
    if [ ${#version[@]} -ne 0 ]; then
      if [ ${#version[@]} -eq 1 ]; then version[1]=0; fi
      if [ ${#version[@]} -eq 2 ]; then version[2]=0; fi
      if [ ${version[0]} -gt $3 ]; then
        echo "Error: problematic $2 symbol " $sym
        exit 1
      fi
      if [ ${version[0]} -lt $3 ]; then continue; fi

      if [ ${version[1]} -gt $4 ]; then
        echo "Error: problematic $2 symbol " $sym
        exit 1
      fi
      if [ ${version[1]} -lt $4 ]; then continue; fi

      if [ ${version[2]} -gt $5 ]; then
        echo "Error: problematic $2 symbol " $sym
        exit 1
      fi
    fi
  done
}

function check_lib_dependency_error
{
for lib in "$1"
do
  if [ -n "`ldd $lib | fgrep $2`" ]; then
    echo "Error: dependency to '$2' found"
    exit 3
  fi
done
}

function get_so_version
{
  local version=`ldd $1 | fgrep $2 | sed "s/.*\.so\.\([0-9\.]\+\).*/\1/"`
  echo "$version"
}

#### Set variables for script ####

ROOT_DIR=$PWD
# DEP_DIR=$ROOT_DIR/build/install
DEP_DIR=$ROOT_DIR/deps
DEP_BUILD_DIR=$ROOT_DIR/build_deps
THREADS=`nproc`

rm -rf build

#### Build dependencies ####

mkdir $DEP_BUILD_DIR
cd $DEP_BUILD_DIR

# NOTE(jda) - Some Linux OSs need to have lib/ on LD_LIBRARY_PATH at build time
export LD_LIBRARY_PATH=$DEP_DIR/lib:${LD_LIBRARY_PATH}

cmake --version

cmake \
  "$@" \
  -D BUILD_JOBS=$THREADS \
  -D BUILD_DEPENDENCIES_ONLY=ON \
  -D CMAKE_INSTALL_PREFIX=$DEP_DIR \
  -D CMAKE_INSTALL_LIBDIR=lib \
  -D BUILD_EMBREE_FROM_SOURCE=OFF \
  -D BUILD_OIDN=ON \
  -D BUILD_OIDN_FROM_SOURCE=OFF \
  -D BUILD_OSPRAY_MODULE_MPI=ON \
  -D INSTALL_IN_SEPARATE_DIRECTORIES=OFF \
  ../scripts/superbuild

cmake --build .
cmake --install . --prefix $ROOT_DIR/build/install

cd $ROOT_DIR
