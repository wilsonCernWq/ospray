// Copyright 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Builder.h"
#include "Noise.h"
#include "ospray_testing.h"
#include "rkcommon/math/AffineSpace.h"
#include "rkcommon/tasking/parallel_for.h"
#include "rkcommon/utility/multidim_index_sequence.h"
#include "rkcommon/utility/random.h"

#include "openvkl/openvkl.h"

// #if defined(__cplusplus)
// #if defined(_WIN32)
// #include <windows.h>
// #else // Platform: Unix
// #include <fcntl.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <sys/mman.h>
// #include <sys/stat.h>
// #include <sys/types.h>
// #include <unistd.h>
// #endif
// #endif

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
  std::pair<cpp::Volume, range1f> createProceduralVolume(
      std::function<bool(vec3f p)> density) const;

  cpp::VolumetricModel createVolumetricModel(
      std::pair<cpp::Volume, range1f> volume, cpp::TransferFunction tfn) const;

  float densityScale{10.f};
  float anisotropy{0.f};
  float gradientShadingScale{1.f};
};

// Inlined definitions ////////////////////////////////////////////////////////

std::pair<cpp::Volume, range1f> RawVolume::createProceduralVolume(
    std::function<bool(vec3f p)> density) const
{
  vec3ul dims{128}; // should be at least 2
  const float spacing = 3.f / (reduce_max(dims) - 1);
  cpp::Volume volume("structuredRegular");

  // generate volume data
  auto numVoxels = dims.product();
  std::vector<float> voxels(numVoxels, 0);
  tasking::parallel_for(dims.z, [&](uint64_t z) {
    for (uint64_t y = 0; y < dims.y; ++y) {
      for (uint64_t x = 0; x < dims.x; ++x) {
        vec3f p = vec3f(x + 0.5f, y + 0.5f, z + 0.5f) / dims;
        if (density(p))
          voxels[dims.x * dims.y * z + dims.x * y + x] =
              0.5f + 0.5f * PerlinNoise::noise(p, 12);
      }
    }
  });

  // calculate voxel range
  range1f voxelRange;
  std::for_each(voxels.begin(), voxels.end(), [&](float &v) {
    if (!std::isnan(v))
      voxelRange.extend(v);
  });

  volume.setParam("data", cpp::CopiedData(voxels.data(), dims));
  volume.setParam("gridOrigin", vec3f(-1.5f, -1.5f, -1.5f));
  volume.setParam("gridSpacing", vec3f(spacing));
  volume.commit();

  return std::make_pair(volume, voxelRange);
}

cpp::VolumetricModel RawVolume::createVolumetricModel(
    std::pair<cpp::Volume, range1f> volume, cpp::TransferFunction tfn) const
{
  tfn.setParam("valueRange", volume.second.toVec2());
  tfn.commit();

  cpp::VolumetricModel volumeModel(volume.first);
  volumeModel.setParam("densityScale", densityScale);
  volumeModel.setParam("anisotropy", anisotropy);
  volumeModel.setParam("transferFunction", tfn);
  volumeModel.setParam("gradientShadingScale", gradientShadingScale);
  volumeModel.commit();

  return volumeModel;
}

void RawVolume::commit()
{
  Builder::commit();

  densityScale = getParam<float>("densityScale", 20.f);
  anisotropy = getParam<float>("anisotropy", 0.f);
  gradientShadingScale =
      getParam<float>("gradientShadingScale", gradientShadingScale);
}

cpp::Group RawVolume::buildGroup() const
{
  cpp::Group group;

  cpp::TransferFunction tfn0 = makeTransferFunction({0.f, 1.f});

  cpp::TransferFunction tfn1("piecewiseLinear");
  {
    std::vector<vec3f> colors = {
        vec3f(0.f, 0.0f, 0.f),
        vec3f(1.f, 0.f, 0.f),
        vec3f(0.f, 1.f, 1.f),
        vec3f(1.f, 1.f, 1.f),
    };

    std::vector<float> opacities = {
        {0.f, 0.33f, 0.66f, 1.f},
    };

    tfn1.setParam("color", cpp::CopiedData(colors));
    tfn1.setParam("opacity", cpp::CopiedData(opacities));
    tfn1.commit();
  }

  cpp::TransferFunction tfn2("piecewiseLinear");
  {
    std::vector<vec3f> colors = {
        vec3f(0.0, 0.0, 0.0),
        vec3f(1.0, 0.65, 0.0),
        vec3f(0.12, 0.6, 1.0),
        vec3f(1.0, 1.0, 1.0),
    };

    std::vector<float> opacities = {
        {0.f, 0.33f, 0.66f, 1.f},
    };

    tfn2.setParam("color", cpp::CopiedData(colors));
    tfn2.setParam("opacity", cpp::CopiedData(opacities));
    tfn2.commit();
  }

  std::vector<cpp::VolumetricModel> volumetricModels;
  {
    volumetricModels.emplace_back(createVolumetricModel(
        createProceduralVolume(
            [](const vec3f &p) { return turbulentSphere(p, 1.f); }),
        tfn0));

    volumetricModels.emplace_back(createVolumetricModel(
        createProceduralVolume(
            [](const vec3f &p) { return turbulentTorus(p, 1.f, 0.375f); }),
        tfn0));

    for (auto volumetricModel : volumetricModels)
      volumetricModel.commit();

    if (!volumetricModels.empty())
      group.setParam("volume", cpp::CopiedData(volumetricModels));
  }

  group.commit();

  return group;
}

cpp::World RawVolume::buildWorld() const
{
  auto group = buildGroup();

  std::vector<cpp::Instance> instanceHandles;
  {
    cpp::Instance instance(group);
    rkcommon::math::AffineSpace3f xform(
        rkcommon::math::LinearSpace3f::scale(1.f), vec3f(0.f, 0.f, 0.f));
    instance.setParam("xfm", xform);
    instance.commit();

    instanceHandles.push_back(instance);

    if (addPlane)
      instanceHandles.push_back(makeGroundPlane(instance.getBounds<box3f>()));
  }

  std::vector<cpp::Light> lightHandles;
  {
    cpp::Light quadLight("quad");
    quadLight.setParam("position", vec3f(-4.0f, 3.0f, 1.0f));
    quadLight.setParam("edge1", vec3f(0.f, 0.0f, -1.0f));
    quadLight.setParam("edge2", vec3f(1.0f, 0.5, 0.0f));
    quadLight.setParam("intensity", 50.0f);
    quadLight.setParam("color", vec3f(2.6f, 2.5f, 2.3f));
    quadLight.commit();
    lightHandles.push_back(quadLight);

    cpp::Light ambientLight("ambient");
    ambientLight.setParam("intensity", 0.4f);
    ambientLight.setParam("color", vec3f(1.f));
    ambientLight.setParam("visible", false);
    ambientLight.commit();
    lightHandles.push_back(ambientLight);
  }

  cpp::World world;
  world.setParam("instance", cpp::CopiedData(instanceHandles));
  world.setParam("light", cpp::CopiedData(lightHandles));

  return world;
}

OSP_REGISTER_TESTING_BUILDER(RawVolume, raw_volume);

} // namespace testing
} // namespace ospray
