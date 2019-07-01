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

// ospray
#include "GeometricModel.ih"
#include "common/Ray.ih"
#include "math/box.ih"
#include "math/vec.ih"

export void *uniform GeometricModel_create(void *uniform cppE,
                                           void *uniform geom)
{
  GeometricModel *uniform self = uniform new GeometricModel;

  self->geom    = (Geometry * uniform) geom;
  self->areaPDF = 0.f;

  return self;
}

export void GeometricModel_set(void *uniform _self,
                               void *uniform primitiveColors,
                               void *uniform prim_materialID)
{
  GeometricModel *uniform self = (GeometricModel * uniform) _self;

  self->primitiveColors = (vec4f * uniform) primitiveColors;
  self->prim_materialID = (uint32 * uniform) prim_materialID;
}

export void GeometricModel_setMaterialList(void *uniform _inst,
                                           uniform int numMaterials,
                                           void *uniform _matList)
{
  GeometricModel *uniform inst = (GeometricModel * uniform) _inst;

  inst->numMaterials = numMaterials;
  inst->materialList = (Material * uniform * uniform) _matList;
}
