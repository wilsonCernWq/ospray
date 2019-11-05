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

#undef NDEBUG

// ospray
#include "Curves.h"
#include "common/Data.h"
#include "common/World.h"
// ispc-generated files
#include "Curves_ispc.h"
#include "ospcommon/utility/DataView.h"
#include <map>

namespace ospray {
  static std::map<std::pair<OSPCurveType, OSPCurveBasis>, RTCGeometryType>
      curveMap = {
          {{OSP_ROUND,  OSP_LINEAR},  (RTCGeometryType)-1},
          {{OSP_FLAT,   OSP_LINEAR},  RTC_GEOMETRY_TYPE_FLAT_LINEAR_CURVE},
          {{OSP_RIBBON, OSP_LINEAR},  (RTCGeometryType)-1},

          {{OSP_ROUND,  OSP_BEZIER},  RTC_GEOMETRY_TYPE_ROUND_BEZIER_CURVE},
          {{OSP_FLAT,   OSP_BEZIER},  RTC_GEOMETRY_TYPE_FLAT_BEZIER_CURVE},
          {{OSP_RIBBON, OSP_BEZIER},  RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_BEZIER_CURVE},

          {{OSP_ROUND,  OSP_BSPLINE}, RTC_GEOMETRY_TYPE_ROUND_BSPLINE_CURVE},
          {{OSP_FLAT,   OSP_BSPLINE}, RTC_GEOMETRY_TYPE_FLAT_BSPLINE_CURVE},
          {{OSP_RIBBON, OSP_BSPLINE}, RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_BSPLINE_CURVE},

          {{OSP_ROUND,  OSP_HERMITE}, RTC_GEOMETRY_TYPE_ROUND_HERMITE_CURVE},
          {{OSP_FLAT,   OSP_HERMITE}, RTC_GEOMETRY_TYPE_FLAT_HERMITE_CURVE},
          {{OSP_RIBBON, OSP_HERMITE}, RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_HERMITE_CURVE},
          
          {{OSP_ROUND,  OSP_CATMULL_ROM}, RTC_GEOMETRY_TYPE_ROUND_CATMULL_ROM_CURVE},
          {{OSP_FLAT,   OSP_CATMULL_ROM}, RTC_GEOMETRY_TYPE_FLAT_CATMULL_ROM_CURVE},
          {{OSP_RIBBON, OSP_CATMULL_ROM}, RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_CATMULL_ROM_CURVE}};

  std::string Curves::toString() const
  {
    return "ospray::Curves";
  }

  void Curves::commit()
  {
    vertexData = getParam<Data *>("vertex.position");

    colorData = getParamDataT<vec4f>("vertex.color");

    indexData = getParamDataT<uint32_t>("index", true);

    curveType = (OSPCurveType)getParam<int>("type", OSP_UNKNOWN_CURVE_TYPE);
    if (curveType == OSP_UNKNOWN_CURVE_TYPE && !radius) 
      throw std::runtime_error("curves geometry has invalid 'type'");

    curveBasis = (OSPCurveBasis)getParam<int>("basis", OSP_UNKNOWN_CURVE_BASIS);
    if (curveBasis == OSP_UNKNOWN_CURVE_BASIS && !radius)
      throw std::runtime_error("curves geometry has invalid 'basis'");

    radius = getParam<float>("radius");

    if((radius && curveType != OSP_UNKNOWN_CURVE_TYPE) 
      || (radius && curveBasis != OSP_UNKNOWN_CURVE_BASIS)) 
      throw std::runtime_error("curves with constant radius do not support custom curveBasis or curveType");

    normalData = curveType == OSP_RIBBON
        ? getParamDataT<vec3f>("vertex.normal", true) 
        : nullptr;

    tangentData = curveBasis == OSP_HERMITE
        ? getParamDataT<vec4f>("vertex.tangent", true) 
        : nullptr;   

    embreeCurveType = curveMap[std::make_pair(curveType, curveBasis)];

    postCreationInfo(vertexData->size());
  }

  size_t Curves::numPrimitives() const
  {
    return indexData ? indexData->size() : 0;
  }

  LiveGeometry Curves::createEmbreeGeometry()
  {
    auto embreeGeo = rtcNewGeometry(ispc_embreeDevice(),
        radius ? RTC_GEOMETRY_TYPE_USER :
        embreeCurveType); 

    LiveGeometry retval;
    retval.embreeGeometry = embreeGeo;

    if (radius) {
      retval.ispcEquivalent = ispc::CurvesUserGeometry_create(this);
      ispc::CurvesUserGeometry_set(retval.ispcEquivalent,
          retval.embreeGeometry,
          radius,
          ispc(indexData),
          ispc(vertexData),
          ispc(colorData));
    } else {
        rtcSetSharedGeometryBuffer(embreeGeo,
            RTC_BUFFER_TYPE_VERTEX,
            0,
            RTC_FORMAT_FLOAT4,
            vertexData->data(),
            0,
            sizeof(vec4f),
            vertexData->size());
        setEmbreeGeometryBuffer(embreeGeo, RTC_BUFFER_TYPE_INDEX, indexData);   
        setEmbreeGeometryBuffer(embreeGeo, RTC_BUFFER_TYPE_NORMAL, normalData);
        if(embreeCurveType == 40) {
           rtcSetSharedGeometryBuffer(embreeGeo,
            RTC_BUFFER_TYPE_TANGENT,
            0,
            RTC_FORMAT_FLOAT4,
            tangentData->data(),
            0,
            sizeof(vec4f),
            tangentData->size());
            }
        if (colorData) {
            rtcSetGeometryVertexAttributeCount(embreeGeo, 1);
            setEmbreeGeometryBuffer(
            embreeGeo, RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, colorData);}
        retval.ispcEquivalent = ispc::Curves_create(this);
        ispc::Curves_set(retval.ispcEquivalent,
          retval.embreeGeometry,
          colorData,
          indexData->size());
    }

    rtcCommitGeometry(embreeGeo);

    return retval;
  }

  OSP_REGISTER_GEOMETRY(Curves, curves);
  OSP_REGISTER_GEOMETRY(Curves, streamlines);

}  // namespace ospray
