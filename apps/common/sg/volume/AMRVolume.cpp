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

/*! \file sg/AMRVolume.cpp node for reading and rendering amr files */

#include "AMRVolume.h"
// ospcommon
#include "ospcommon/utility/ArrayView.h"
// sg
#include "../importer/Importer.h"
// core ospray
#include "ospray/common/OSPCommon.h"

namespace ospray {
  namespace sg {

    AMRVolume::AMRVolume()
    {
      createChild("maxLevel", "int", 1 << 30);
      createChild("amrMethod", "string", std::string("current"))
          .setWhiteList({std::string("current"),
                         std::string("currentLevel"),
                         std::string("octant"),
                         std::string("finest"),
                         std::string("finestLevel")});
      createChild("voxelType", "string", std::string("unspecified"));
    }

    AMRVolume::~AMRVolume()
    {
      for (auto *ptr : brickPtrs)
        delete [] ptr;
    }

    std::string AMRVolume::toString() const
    {
      return "ospray::sg::AMRVolume";
    }

    void AMRVolume::parseRaw2AmrFile(const FileName &fileName,
                                     int BS,
                                     int maxLevel)
    {
      char *maxLevelEnv = getenv("AMR_MAX_LEVEL");
      if (maxLevelEnv) {
        maxLevel = atoi(maxLevelEnv);
        ospLogF(1) << "will only parse amr file up to level " << maxLevel
                   << std::endl;
      }

      FileName infoFileName = fileName.str() + std::string(".info");
      FileName dataFileName = fileName.str() + std::string(".data");

      FILE *infoFile = fopen(infoFileName.c_str(), "rb");
      FILE *dataFile = fopen(dataFileName.c_str(), "rb");

      if (infoFile == nullptr) {
        throw std::runtime_error(std::string("#osp:sg - ERROR could not open '")
                                 + infoFileName.c_str() + "'");
      }

      if (dataFile == nullptr) {
        throw std::runtime_error(std::string("#osp:sg - ERROR could not open '")
                                 + dataFileName.c_str() + "'");
      }

      BrickInfo bi;
      auto bounds = child("bounds").valueAs<box3f>();
      auto numCells = BS * BS * BS;
      while (fread(&bi, sizeof(bi), 1, infoFile)) {
        utility::ArrayView<float> bd(new float[numCells], numCells);

        int nr = fread(bd.data(), sizeof(float), numCells, dataFile);
        UNUSED(nr);

        if (bi.level > maxLevel) {
          delete [] bd.data();
          continue;
        }
        brickInfo.push_back(bi);
        bounds.extend((vec3f(bi.box.upper) + vec3f(1.f)) * bi.dt);

        assert(nr == numCells);
        for (const auto &c : bd)
          valueRange.extend(c);
        brickPtrs.push_back(bd);
      }

      child("bounds") = bounds;
      child("transferFunction")["valueRange"] = valueRange.toVec2f();
      ospLogF(1) << "read file; found " << brickInfo.size() << " bricks"
                 << std::endl;

      fclose(infoFile);
      fclose(dataFile);
    }

    void AMRVolume::preCommit(RenderContext &ctx)
    {
      auto volume = valueAs<OSPVolume>();

      if (!volume) {
        volume = ospNewVolume("amr_volume");
        setValue(volume);
      }

      for (size_t bID = 0; bID < brickInfo.size(); bID++) {
        const auto &bi = brickInfo[bID];
        OSPData data = ospNewData(bi.size().product(),
                                  OSP_FLOAT,
                                  this->brickPtrs[bID],
                                  OSP_DATA_SHARED_BUFFER);
        this->brickData.push_back(data);
      }

      auto brickDataNode =
          std::make_shared<DataArrayOSP>((OSPObject*)brickData.data(),
                                         brickData.size(),
                                         false);
      brickDataNode->setName("brickData");
      brickDataNode->setType("DataArrayOSP");
      add(brickDataNode);

      auto brickInfoNode =
          std::make_shared<DataArrayRAW>((byte_t*)brickInfo.data(),
                                         brickInfo.size() * sizeof(BrickInfo),
                                         false);
      brickInfoNode->setName("brickInfo");
      brickInfoNode->setType("DataArrayRAW");
      add(brickInfoNode);

      // NOTE(jda) - Hack! there are issues with adding data array nodes
      //             *during* sg traversal...remove this when setFromXML() is
      //             changed
      brickDataNode->postCommit(ctx);
      brickInfoNode->postCommit(ctx);

      child("voxelRange") = valueRange.toVec2f();
    }

    OSP_REGISTER_SG_NODE(AMRVolume);
  }
}
