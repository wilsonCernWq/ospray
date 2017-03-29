// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
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

#pragma once

// ospcommon 
#include "ospcommon/AffineSpace.h"

// ospray API
#include "ospray/ospray.h"

namespace ospray {
  namespace sg {
    using namespace ospcommon;

#define THROW_SG_ERROR(err) \
    throw std::runtime_error("in "+std::string(__PRETTY_FUNCTION__)+":"+std::string(err))

    typedef unsigned int uint;
    
    /*! base node for every scene graph node */
    struct Node;
    /*! c++ wrapper for an ospray model */
    struct World;
    /*! c++ wrapper for an ospray light source */
    struct Light;
    /*! c++ wrapper for an ospray camera */
    struct Geometry;
    /*! c++ wrapper for an ospray geometry type */
    struct Volume;
    /*! c++ wrapper for an ospray volume type */
    struct Camera;
    /*! c++ wrapper for an ospray renderer (the thing that produces
        pixel colors is actualy close to what PBRT calls in
        'integrator' */
    struct Integrator;

    struct RenderContext;

    //! map the given file to memory and return that pointer
    const unsigned char* mapFile(const std::string &fileName);

    /*! return the size (in byte) for a given ospray data type */
    size_t sizeOf(const OSPDataType type);

    /*! return the ospray data type for a given string-ified type */
    OSPDataType getOSPDataTypeFor(const char *typeName);

    /*! return the ospray data type for a given string-ified type */
    OSPDataType getOSPDataTypeFor(const std::string &typeName);

  } // ::ospray::sg
} // ::ospray


