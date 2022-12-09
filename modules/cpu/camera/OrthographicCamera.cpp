// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "OrthographicCamera.h"
#ifndef OSPRAY_TARGET_SYCL
// ispc exports
#include "camera/OrthographicCamera_ispc.h"
#else
namespace ispc {
void *OrthographicCamera_initRay_addr();
void OrthographicCamera_projectBox(
    void *_self, const void *_box, void *_projection);
} // namespace ispc
#endif

namespace ospray {

OrthographicCamera::OrthographicCamera(api::ISPCDevice &device)
    : AddStructShared(device.getIspcrtDevice(), device)
{
#ifndef OSPRAY_TARGET_SYCL
  getSh()->super.initRay = reinterpret_cast<ispc::Camera_initRay>(
      ispc::OrthographicCamera_initRay_addr());
#endif
}

std::string OrthographicCamera::toString() const
{
  return "ospray::OrthographicCamera";
}

void OrthographicCamera::commit()
{
  Camera::commit();

  height = getParam<float>("height", 1.f);
  aspect = getParam<float>("aspect", 1.f);

  vec2f size = height;
  size.x *= aspect;

  if (getSh()->super.motionBlur) {
    getSh()->dir = dir;
    getSh()->du_size = vec3f(size.x, size.y, 1.0f);
    getSh()->dv_up = up;
    getSh()->org = pos;
  } else {
    getSh()->dir = normalize(dir);
    getSh()->du_size = normalize(cross(getSh()->dir, up));
    getSh()->dv_up = cross(getSh()->du_size, getSh()->dir) * size.y;
    getSh()->du_size = getSh()->du_size * size.x;
    getSh()->org =
        pos - 0.5f * getSh()->du_size - 0.5f * getSh()->dv_up; // shift
  }
}

box3f OrthographicCamera::projectBox(const box3f &b) const
{
  box3f projection;
  ispc::OrthographicCamera_projectBox(getSh(), &b, &projection);
  return projection;
}

} // namespace ospray
