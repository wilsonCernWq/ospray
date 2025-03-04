## Copyright 2009 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

add_library(ospray_mpi_common SHARED
    Collectives.cpp
    MPIBcastFabric.cpp
    MPICommon.cpp
    ObjectHandle.cpp
    Profiling.cpp
    maml/maml.cpp
    maml/Context.cpp
)

include(GenerateExportHeader)
generate_export_header(ospray_mpi_common)

target_link_libraries(ospray_mpi_common
  PRIVATE
    Snappy::snappy
  PUBLIC
    rkcommon::rkcommon
    ospray
    MPI::MPI_CXX
)

target_include_directories(ospray_mpi_common
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

if (OSPRAY_MPI_ENABLE_PROFILING)
  target_compile_definitions(ospray_mpi_common
    PUBLIC
      -DENABLE_PROFILING
  )
endif()

ospray_install_library(ospray_mpi_common mpi)
