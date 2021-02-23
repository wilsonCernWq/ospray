## Copyright 2018-2020 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

if (NOT OSPRAY_APPS_EXAMPLES)
  return()
endif()

# prefer libGL over libOpenGl for better compatibility with SWR
set(OpenGL_GL_PREFERENCE "LEGACY")
find_package(OpenGL 2 REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(ospSimulator
  ${OSPRAY_RESOURCE}
  tfn/TransferFunctionModule.cpp
  tfn/TransferFunctionWidget.cpp
  GLFWOSPRayWindow.cpp
  imgui_impl_glfw_gl3.cpp
  ospSimulator.cpp
)

target_link_libraries(ospSimulator
PRIVATE
  arcball_camera
  ospray_testing
  ospray_imgui
  glfw
  ${OPENGL_LIBRARIES}
)

ospray_sign_target(ospSimulator)

install(TARGETS ospSimulator
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT apps
)
