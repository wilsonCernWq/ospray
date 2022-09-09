// Copyright 2018 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

/* This larger example shows how to use the MPIDistributedDevice to write an
 * interactive rendering application, which shows a UI on rank 0 and uses
 * all ranks in the MPI world for data loading and rendering. Each rank
 * generates a local sub-brick of volume data, as if rendering some
 * large distributed dataset.
 */

#include <imgui.h>
#include <mpi.h>
#include <iterator>
#include <memory>
#include <random>
#include <fstream>
#include "GLFWDistribOSPRayWindow.h"
#include "ospray/ospray_cpp.h"
#include "ospray/ospray_cpp/ext/rkcommon.h"
#include "ospray/ospray_util.h"

using namespace ospray;
using namespace rkcommon;
using namespace rkcommon::math;

struct VolumeBrick
{
  // the volume data itself
  cpp::Volume brick;
  cpp::VolumetricModel model;
  cpp::Group group;
  cpp::Instance instance;
  // the bounds of the owned portion of data
  box3f bounds;
  // the full bounds of the owned portion + ghost voxels
  box3f ghostBounds;
};

static box3f worldBounds;

// Generate the rank's local volume brick
VolumeBrick makeLocalVolume(const int mpiRank, const int mpiWorldSize);

int main(int argc, char **argv)
{
  int mpiThreadCapability = 0;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &mpiThreadCapability);
  if (mpiThreadCapability != MPI_THREAD_MULTIPLE
      && mpiThreadCapability != MPI_THREAD_SERIALIZED) {
    fprintf(stderr,
        "OSPRay requires the MPI runtime to support thread "
        "multiple or thread serialized.\n");
    return 1;
  }

  int mpiRank = 0;
  int mpiWorldSize = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpiWorldSize);

  std::cout << "OSPRay rank " << mpiRank << "/" << mpiWorldSize << "\n";

  // load the MPI module, and select the MPI distributed device. Here we
  // do not call ospInit, as we want to explicitly pick the distributed
  // device. This can also be done by passing --osp:mpi-distributed when
  // using ospInit, however if the user doesn't pass this argument your
  // application will likely not behave as expected
  ospLoadModule("mpi");

  {
    cpp::Device mpiDevice("mpiDistributed");
    mpiDevice.commit();
    mpiDevice.setCurrent();

    // set an error callback to catch any OSPRay errors and exit the application
    ospDeviceSetErrorCallback(
        mpiDevice.handle(),
        [](void *, OSPError error, const char *errorDetails) {
          std::cerr << "OSPRay error: " << errorDetails << std::endl;
          exit(error);
        },
        nullptr);

    // all ranks specify the same rendering parameters, with the exception of
    // the data to be rendered, which is distributed among the ranks
    VolumeBrick brick = makeLocalVolume(mpiRank, mpiWorldSize);

    // create the "world" model which will contain all of our geometries
    cpp::World world;
    world.setParam("instance", cpp::CopiedData(brick.instance));

    world.setParam("region", cpp::CopiedData(brick.bounds));
    world.commit();

    // create OSPRay renderer
    cpp::Renderer renderer("mpiRaycast");

    // create and setup an ambient light
    cpp::Light ambientLight("ambient");
    ambientLight.commit();
    renderer.setParam("light", cpp::CopiedData(ambientLight));

    // create a GLFW OSPRay window: this object will create and manage the
    // OSPRay frame buffer and camera directly
    auto glfwOSPRayWindow =
        std::unique_ptr<GLFWDistribOSPRayWindow>(new GLFWDistribOSPRayWindow(
            vec2i{1024, 768}, worldBounds, world, renderer));

    int spp = 1;
    int currentSpp = 1;
    if (mpiRank == 0) {
      glfwOSPRayWindow->registerImGuiCallback(
          [&]() { ImGui::SliderInt("pixelSamples", &spp, 1, 64); });
    }

    glfwOSPRayWindow->registerDisplayCallback(
        [&](GLFWDistribOSPRayWindow *win) {
          // Send the UI changes out to the other ranks so we can synchronize
          // how many samples per-pixel we're taking
          MPI_Bcast(&spp, 1, MPI_INT, 0, MPI_COMM_WORLD);
          if (spp != currentSpp) {
            currentSpp = spp;
            renderer.setParam("pixelSamples", spp);
            win->addObjectToCommit(renderer.handle());
          }
        });

    // start the GLFW main loop, which will continuously render
    glfwOSPRayWindow->mainLoop();
  }
  // cleanly shut OSPRay down
  ospShutdown();

  MPI_Finalize();

  return 0;
}

bool computeDivisor(int x, int &divisor)
{
  int upperBound = std::sqrt(x);
  for (int i = 2; i <= upperBound; ++i) {
    if (x % i == 0) {
      divisor = i;
      return true;
    }
  }
  return false;
}

// Compute an X x Y x Z grid to have 'num' grid cells,
// only gives a nice grid for numbers with even factors since
// we don't search for factors of the number, we just try dividing by two
vec3i computeGrid(int num)
{
  vec3i grid(1);
  int axis = 0;
  int divisor = 0;
  while (computeDivisor(num, divisor)) {
    grid[axis] *= divisor;
    num /= divisor;
    axis = (axis + 1) % 3;
  }
  if (num != 1) {
    grid[axis] *= num;
  }
  return grid;
}

static std::string localVolumeFiles[] = {
  "/mnt/c/Users/wilso/Documents/Projects/research/instant-vnr-python/data/decoded_rank0_0_0_0_640_220_116.raw",
  "/mnt/c/Users/wilso/Documents/Projects/research/instant-vnr-python/data/decoded_rank1_0_0_115_640_220_229.raw",
};

static vec3i globalDims(640,220,229);

static box3i localVolumeBounds[] = {
  box3i(vec3i(0,0,0),   vec3i(640,220,116)),
  box3i(vec3i(0,0,116), vec3i(640,220,229)),
};

static box3i localVolumeGhostDims[] = {
  box3i(vec3i(0,0,0),   vec3i(640,220,117)),
  box3i(vec3i(0,0,115), vec3i(640,220,229)),
};

static size_t localVolumeOffsets[] = {
  0,
  size_t(115) * 640 * 220
};

static std::vector<float> readBinaryFile(std::string fileName, size_t size, size_t offset)
{
  std::vector<float> ret; //std::list may be preferable for large files
  ret.resize(size); // allocate memory for an array 

  std::fstream file;
  file.open(fileName, std::ios::in | std::ios::binary);
  file.seekg(offset * sizeof(float));
  file.read((char*)ret.data(), size * sizeof(float));
  if (!file)
    std::cout << "error: only " << file.gcount() << " could be read";
  file.close();
  return ret;
}

VolumeBrick makeLocalVolume(const int mpiRank, const int mpiWorldSize)
{
  // const vec3i grid = computeGrid(mpiWorldSize);
  // const vec3i brickId(mpiRank % grid.x,
  //     (mpiRank / grid.x) % grid.y,
  //     mpiRank / (grid.x * grid.y));
  // // The bricks are 64^3 + 1 layer of ghost voxels on each axis
  // const vec3i brickVolumeDims = vec3i(32);
  // const vec3i brickGhostDims = vec3i(brickVolumeDims + 2);

  // const vec3i brickVolumeDims = localVolumeBounds[mpiRank].size();
  const vec3i brickGhostDims = localVolumeGhostDims[mpiRank].size();

  // // The grid is over the [0, grid * brickVolumeDims] box
  // worldBounds = box3f(vec3f(0.f), vec3f(grid * brickVolumeDims));
  // const vec3f brickLower = brickId * brickVolumeDims;
  // const vec3f brickUpper = brickId * brickVolumeDims + brickVolumeDims;
  worldBounds = box3f(vec3f(0.f), globalDims);

  VolumeBrick brick;
  // brick.bounds = box3f(brickLower, brickUpper);
  brick.bounds = box3f(localVolumeBounds[mpiRank]);

  // we just put ghost voxels on all sides here, but a real application
  // would change which faces of each brick have ghost voxels dependent
  // on the actual data
  // brick.ghostBounds = box3f(brickLower - vec3f(1.f), brickUpper + vec3f(1.f));
  brick.ghostBounds = box3f(localVolumeGhostDims[mpiRank]);

  brick.brick = cpp::Volume("structuredRegular");
  brick.brick.setParam("cellCentered", true);
  brick.brick.setParam("dimensions", brickGhostDims);

  // we use the grid origin to place this brick in the right position inside
  // the global volume
  brick.brick.setParam("gridOrigin", brick.ghostBounds.lower);

  // generate the volume data to just be filled with this rank's id
  const size_t nVoxels = brickGhostDims.x * brickGhostDims.y * brickGhostDims.z;

  // std::vector<float> volumeData(nVoxels, 0.1);

  // std::vector<float> volumeData = readBinaryFile(localVolumeFiles[mpiRank], nVoxels, 0);
  std::vector<float> volumeData = readBinaryFile("/mnt/c/Users/wilso/Documents/Projects/research/instant-vnr-python/data/mechhand-5000.raw", 
                                                 nVoxels, localVolumeOffsets[mpiRank]);

  brick.brick.setParam("data",
      cpp::CopiedData(static_cast<const float *>(volumeData.data()),
          vec3ul(brickGhostDims)));

  brick.brick.commit();

  brick.model = cpp::VolumetricModel(brick.brick);
  cpp::TransferFunction tfn("piecewiseLinear");
  std::vector<vec3f> colors = {vec3f(0.f, 0.f, 1.f), vec3f(1.f, 0.f, 0.f)};
  std::vector<float> opacities = {0.0f, 1.f};

  tfn.setParam("color", cpp::CopiedData(colors));
  tfn.setParam("opacity", cpp::CopiedData(opacities));
  // color the bricks by their rank, we pad the range out a bit to keep
  // any brick from being completely transparent
  vec2f valueRange = vec2f(0, 1.0);
  tfn.setParam("valueRange", valueRange);
  tfn.commit();
  brick.model.setParam("transferFunction", tfn);
  brick.model.setParam("samplingRate", 10.f);
  brick.model.commit();

  brick.group = cpp::Group();
  brick.group.setParam("volume", cpp::CopiedData(brick.model));
  brick.group.commit();

  brick.instance = cpp::Instance(brick.group);
  brick.instance.commit();

  return brick;
}
