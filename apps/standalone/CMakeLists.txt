## Copyright 2018-2020 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

# build ospTutorial, for demonstration and API testing
add_executable(standalone ${OSPRAY_RESOURCE} standalone.cpp)
target_link_libraries(standalone PRIVATE ospray)
