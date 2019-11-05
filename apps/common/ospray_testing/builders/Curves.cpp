// ======================================================================== //
// Copyright 2009-2019 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "Builder.h"
#include "ospray_testing.h"
#include <iostream>
// stl
#include <random>

using namespace ospcommon::math;
using namespace std;

namespace ospray {
  namespace testing {

    struct Curves : public detail::Builder
    {
      Curves()           = default;
      ~Curves() override = default;

      void commit() override;

      cpp::Group buildGroup() const override;

      cpp::World buildWorld() const override;

      std::string curveBasis;
    };

    static std::vector<vec4f> points = {
      {-1.0f, 0.0f, -2.f, 0.2f},
      {0.0f, -1.0f, 0.0f, 0.2f},
      {1.0f, 0.0f, 2.f, 0.2f},
      {-1.0f, 0.0f, 2.f, 0.2f},
      {0.0f, 1.0f, 0.0f, 0.6f},
      {1.0f, 0.0f, -2.f, 0.2f},
      {-1.0f, 0.0f, -2.f, 0.2f},
      {0.0f, -1.0f, 0.0f, 0.2f},
      {1.0f, 0.0f, 2.f, 0.2f}
    };
    static std::vector<vec3f> points3f = {
      {-1.0f, 0.0f, -2.f},
      {0.0f, -1.0f, 0.0f},
      {1.0f, 0.0f, 2.f},
      {-1.0f, 0.0f, 2.f},
      {0.0f, 1.0f, 0.0f},
      {1.0f, 0.0f, -2.f},
      {-1.0f, 0.0f, -2.f},
      {0.0f, -1.0f, 0.0f},
      {1.0f, 0.0f, 2.f}
    };
    static std::vector<vec4f> colors = {
      {1.0f, 0.0f, 0.0f, 0.0f},
      {1.0f, 1.0f, 0.0f, 0.0f},
      {0.0f, 1.0f, 0.0f, 0.0f},
      {0.0f, 1.0f, 1.0f, 0.0f},
      {0.0f, 0.0f, 1.0f, 0.0f},
      {1.0f, 0.0f, 1.0f, 0.0f},
      {0.0f, 1.0f, 1.0f, 0.0f},
      {0.0f, 0.0f, 1.0f, 0.0f},
      {1.0f, 0.0f, 1.0f, 0.0f}
    };
    static std::vector<unsigned int> indices = {0, 1, 2, 3, 4, 5};

    void Curves::commit()
    {
      Builder::commit();

      curveBasis = getParam<std::string>("curveBasis", "bspline");
    }

    cpp::Group Curves::buildGroup() const
    {
      std::vector<cpp::GeometricModel> geometricModels;
      
      cpp::Geometry geom("curves");

      if(curveBasis == "hermite") {
        geom.setParam("type", int(OSP_ROUND));
        geom.setParam("basis", int(OSP_HERMITE));
        std::vector<vec4f> tangents;
        for(auto iter = points.begin(); iter != points.end() - 1; ++iter) {
          const vec4f pointTangent = *(iter+1) - *iter;
          tangents.push_back(pointTangent);
        }
        geom.setParam("vertex.position", cpp::Data(points));
        geom.setParam("vertex.tangent", cpp::Data(tangents));
      } else if (curveBasis == "catmull-rom") {
        geom.setParam("type", int(OSP_ROUND));
        geom.setParam("basis", int(OSP_CATMULL_ROM));
        geom.setParam("vertex.position", cpp::Data(points));
      } else if (curveBasis == "streamlines") {
        geom.setParam("radius", 0.1f);
        geom.setParam("vertex.position", cpp::Data(points));
      } else {
        geom.setParam("type", int(OSP_ROUND));
        geom.setParam("basis", int(OSP_BSPLINE));
        geom.setParam("vertex.position", cpp::Data(points));
     }
      geom.setParam("vertex.color", cpp::Data(colors));
      geom.setParam("index", cpp::Data(indices));
      geom.commit();

      cpp::Material mat(rendererType, "ThinGlass");
      mat.setParam("attenuationDistance", 0.2f);
      mat.commit();

      cpp::GeometricModel model(geom);
      model.setParam("material", cpp::Data(mat));
      model.commit();

      cpp::Group group;

      group.setParam("geometry", cpp::Data(model));
      group.commit();

      return group;
    }

    cpp::World Curves::buildWorld() const {
    
      cpp::World world;
      auto group = buildGroup();

      cpp::Instance inst(group);
      inst.commit();

      world.setParam("instance", cpp::Data(inst));

      cpp::Light light("ambient");
      light.commit();

      world.setParam("light", cpp::Data(light));

      return world;
    }

    OSP_REGISTER_TESTING_BUILDER(Curves, curves);

  }  // namespace testing
}  // namespace ospray
