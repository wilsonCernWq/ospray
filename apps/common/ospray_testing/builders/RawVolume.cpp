// Copyright 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Builder.h"
#include "ospray_testing.h"
#include "rkcommon/math/AffineSpace.h"
#include "rkcommon/tasking/parallel_for.h"

#include "openvkl/openvkl.h"
#include "openvkl/vdb.h"

#if defined(__cplusplus)
#if defined(_WIN32)
#include <windows.h>
#else // Platform: Unix
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#endif

using namespace rkcommon::math;

namespace ospray {
namespace testing {

// ----------------------------------------------------------------------------
// Builder
// ----------------------------------------------------------------------------
struct RawVolume : public detail::Builder
{
  RawVolume() = default;
  ~RawVolume() override = default;

  void commit() override;

  cpp::Group buildGroup() const override;
  cpp::World buildWorld() const override;

 private:
  float densityScale{1.f};
  float anisotropy{0.f};
  static constexpr uint32_t domainRes = 128;
};

// Inlined definitions ////////////////////////////////////////////////////////

void RawVolume::commit()
{
  Builder::commit();

  densityScale = getParam<float>("densityScale", 20.f);
  anisotropy = getParam<float>("anisotropy", 0.f);
}

cpp::Group RawVolume::buildGroup() const
{
  vec3f gridScaling = (float)domainRes;

  std::string file = "C:/Datasets/bunny_cloud.tamr";
  // std::string file = "C:/Datasets/wdas_cloud.htg";
  // std::string file = "C:/Datasets/bunny_cloud.htg";

  cpp::Volume volume("htg");
  volume.setParam("filter", (int)OSP_VOLUME_FILTER_TRILINEAR);
  volume.setParam("gradientFilter", (int)OSP_VOLUME_FILTER_TRILINEAR);
  volume.setParam("gridOrigin", vec3f(0, 0, 0));
  volume.setParam("gridScaling", gridScaling);
  volume.setParam("nodeFile", file.c_str());
  volume.commit();

  cpp::VolumetricModel model(volume);
  model.setParam("transferFunction", makeTransferFunction({0.f, 1.f}));
  model.setParam("densityScale", densityScale);
  model.setParam("anisotropy", anisotropy);
  model.commit();

  cpp::Group group;
  group.setParam("volume", cpp::CopiedData(model));
  group.commit();

  return group;
}

cpp::World RawVolume::buildWorld() const
{
  auto group = buildGroup();
  cpp::Instance instance(group);
  rkcommon::math::AffineSpace3f xform(
      rkcommon::math::LinearSpace3f::scale(8.f / domainRes),
      vec3f(-4.f, 0, -4.f));
  instance.setParam("xfm", xform);
  instance.commit();

  std::vector<cpp::Instance> inst;
  inst.push_back(instance);

  if (addPlane)
    inst.push_back(makeGroundPlane(instance.getBounds<box3f>()));

  cpp::Light quadLight("quad");
  quadLight.setParam("position", vec3f(-4.f, 8.0f, 4.f));
  quadLight.setParam("edge1", vec3f(0.f, 0.0f, -8.0f));
  quadLight.setParam("edge2", vec3f(2.f, 1.0f, 0.0f));
  quadLight.setParam("intensity", 5.0f);
  quadLight.setParam("color", vec3f(2.8f, 2.2f, 1.9f));
  quadLight.commit();

  std::vector<cpp::Light> lightHandles;
  lightHandles.push_back(quadLight);

  cpp::World world;
  world.setParam("instance", cpp::CopiedData(inst));
  world.setParam("light", cpp::CopiedData(lightHandles));

  return world;
}

OSP_REGISTER_TESTING_BUILDER(RawVolume, htg_volume);

} // namespace testing
} // namespace ospray