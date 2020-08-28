// Copyright 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// ospray
#include "../Renderer.h"

namespace ospray {

struct SciVis : public Renderer
{
  SciVis(int defaultAOSamples = 1, bool defaultShadowsEnabled = false);
  std::string toString() const override;
  void commit() override;

 private:
  int aoSamples{1};
  bool shadowsEnabled{false};
};

} // namespace ospray
