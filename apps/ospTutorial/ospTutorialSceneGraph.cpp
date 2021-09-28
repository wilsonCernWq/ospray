// Copyright 2009-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

/* This is a small example tutorial of how to use OSPRay's async API in an
 * application. We setup up two scenes which are rendered asynchronously
 * in parallel to each other.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#ifdef _WIN32
#include <conio.h>
#include <malloc.h>
#include <windows.h>
#else
#include <alloca.h>
#endif

#include "ospray/ospray_cpp.h"
#include "ospray/ospray_cpp/ext/rkcommon.h"
#include "ospray/ospray_util.h"

#include "common/transferfunction.h"
#include "common/vidi_json.h"
#include "common/volume_readers.h"

#include <fstream>

typedef struct
{
  ospray::cpp::Camera camera;
  ospray::cpp::World world;
  ospray::cpp::Renderer renderer;

  ospray::cpp::FrameBuffer framebuffer;

  std::vector<std::shared_ptr<char[]>> userdata;

  vec2i imgSize;

  typedef ospray::cpp::Future task_t;

  task_t renderAsync()
  {
    return framebuffer.renderFrame(renderer, camera, world);
  }

  float getFrameVariance()
  {
    return framebuffer.variance();
  }

  void map(const uint32_t *&fb)
  {
    fb = (uint32_t *)framebuffer.map(OSP_FB_COLOR);
  }

  void unmap(const void *fb)
  {
    framebuffer.unmap((void *)fb);
  }

  void cleanUp() {}

} context_t;

// helper function to write the rendered image as PPM file
void writePPM(const char *fileName, const vec2i *size, const uint32_t *pixel)
{
  FILE *file = fopen(fileName, "wb");
  if (!file) {
    fprintf(stderr, "fopen('%s', 'wb') failed: %d", fileName, errno);
    return;
  }
  fprintf(file, "P6\n%i %i\n255\n", size->x, size->y);
  unsigned char *out = (unsigned char *)alloca(3 * size->x);
  for (int y = 0; y < size->y; y++) {
    const unsigned char *in =
        (const unsigned char *)&pixel[(size->y - 1 - y) * size->x];
    for (int x = 0; x < size->x; x++) {
      out[3 * x + 0] = in[4 * x + 0];
      out[3 * x + 1] = in[4 * x + 1];
      out[3 * x + 2] = in[4 * x + 2];
    }
    fwrite(out, 3 * size->x, sizeof(char), file);
  }
  fprintf(file, "\n");
  fclose(file);
}

void buildScene1(context_t *context);

void buildScene2(context_t *context);

void buildSceneFile(context_t *context, std::string file);

int main(int argc, const char **argv)
{
#ifdef _WIN32
  int waitForKey = 0;
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
    // detect standalone console: cursor at (0,0)?
    waitForKey = csbi.dwCursorPosition.X == 0 && csbi.dwCursorPosition.Y == 0;
  }
#endif

  // initialize OSPRay; OSPRay parses (and removes) its commandline parameters,
  // e.g. "--osp:debug"
  OSPError init_error = ospInit(&argc, argv);
  if (init_error != OSP_NO_ERROR)
    return init_error;

  std::vector<context_t> contexts;

  contexts.emplace_back();
  contexts[0].imgSize.x = 1024;
  contexts[0].imgSize.y = 768;
  buildSceneFile(&contexts[0], argv[1]);

  // contexts.emplace_back();
  // contexts[0].imgSize.x = 1024;
  // contexts[0].imgSize.y = 768;
  // buildScene1(&contexts[0]);

  contexts.emplace_back();
  contexts[1].imgSize.x = 800;
  contexts[1].imgSize.y = 600;
  buildScene2(&contexts[1]);

  printf("starting renders...\n");
  std::vector<context_t::task_t> tasks;

  // render one frame for each scene
  for (int i = 0; i < 2; ++i) {
    tasks.emplace_back(contexts[i].renderAsync());
  }

  for (int i = 0; i < 2; ++i) {
    int isFinished = tasks[i].isReady(OSP_TASK_FINISHED);
    printf("status of 'futures[%i]' is %i\n", i, isFinished);
  }

  // We don't need to wait for them in the order they were started
  for (int i = 1; i >= 0; --i) {
    tasks[i].wait(OSP_FRAME_FINISHED);
    printf("...done, variance of render %i was %f\n",
        i,
        contexts[i].getFrameVariance());
  }

  tasks.clear();

  // access framebuffer and write its content as PPM file
  const uint32_t *fb;
  contexts[0].map(fb);
  writePPM("firstFrame-scene1.ppm", &contexts[0].imgSize, fb);
  contexts[0].unmap(fb);
  printf("wrote rendered scene 1 to firstFrame-scene1.ppm\n");

  contexts[1].map(fb);
  writePPM("firstFrame-scene2.ppm", &contexts[1].imgSize, fb);
  contexts[1].unmap(fb);
  printf("wrote rendered scene 2 to firstFrame-scene2.ppm\n");

  // render 10 more frames, which are accumulated to result in a better
  // converged image
  printf("starting accumulation...\n");
  for (int frames = 0; frames < 10; frames++) {
    for (int i = 0; i < 2; ++i) {
      tasks.emplace_back(contexts[i].renderAsync());
    }
    for (int i = 0; i < 2; ++i) {
      tasks[i].wait(OSP_FRAME_FINISHED);
    }
    if (frames < 9) // don't release future of last frame yet
      tasks.clear();
  }
  for (int i = 1; i >= 0; --i) {
    printf("...done, variance of render %i is now %f\n",
        i,
        contexts[i].getFrameVariance());
  }
  tasks.clear();

  contexts[0].map(fb);
  writePPM("accumulatedFrame-scene1.ppm", &contexts[0].imgSize, fb);
  contexts[0].unmap(fb);
  printf("wrote accumulated scene 1 to accumulatedFrame-scene1.ppm\n");

  contexts[1].map(fb);
  writePPM("accumulatedFrame-scene2.ppm", &contexts[1].imgSize, fb);
  contexts[1].unmap(fb);
  printf("wrote accumulated scene 2 to accumulatedFrame-scene2.ppm\n");

  // final cleanups
  for (int i = 0; i < 2; ++i) {
    contexts[i].cleanUp();
  }
  contexts.clear();

  printf("shutting down\n");

  ospShutdown();

#ifdef _WIN32
  if (waitForKey) {
    printf("\n\tpress any key to exit");
    _getch();
  }
#endif

  return 0;
}

void buildScene1(context_t *ctx)
{
  // camera
  vec3f cam_pos{0.f, 0.f, 0.f};
  vec3f cam_up{0.f, 1.f, 0.f};
  vec3f cam_view{0.1f, 0.f, 1.f};

  // triangle mesh data
  std::vector<vec3f> vertex = {vec3f(-1.0f, -1.0f, 3.0f),
      vec3f(-1.0f, 1.0f, 3.0f),
      vec3f(1.0f, -1.0f, 3.0f),
      vec3f(0.1f, 0.1f, 0.3f)};

  std::vector<vec4f> color = {vec4f(0.9f, 0.5f, 0.5f, 1.0f),
      vec4f(0.8f, 0.8f, 0.8f, 1.0f),
      vec4f(0.8f, 0.8f, 0.8f, 1.0f),
      vec4f(0.5f, 0.9f, 0.5f, 1.0f)};

  std::vector<vec3ui> index = {vec3ui(0, 1, 2), vec3ui(1, 2, 3)};

  // create and setup camera
  ospray::cpp::Camera camera("perspective");
  camera.setParam("aspect", ctx->imgSize.x / (float)ctx->imgSize.y);
  camera.setParam("position", cam_pos);
  camera.setParam("direction", cam_view);
  camera.setParam("up", cam_up);
  camera.commit(); // commit each object to indicate modifications are done

  // create and setup model and mesh
  ospray::cpp::Geometry mesh("mesh");
  mesh.setParam("vertex.position", ospray::cpp::CopiedData(vertex));
  mesh.setParam("vertex.color", ospray::cpp::CopiedData(color));
  mesh.setParam("index", ospray::cpp::CopiedData(index));
  mesh.commit();

  // put the mesh into a model
  ospray::cpp::GeometricModel model(mesh);
  model.commit();

  // put the model into a group (collection of models)
  ospray::cpp::Group group;
  group.setParam("geometry", ospray::cpp::CopiedData(model));
  group.commit();

  // put the group into an instance (give the group a world transform)
  ospray::cpp::Instance instance(group);
  instance.commit();

  // put the instance in the world
  ospray::cpp::World world;
  world.setParam("instance", ospray::cpp::CopiedData(instance));

  // create and setup light for Ambient Occlusion
  ospray::cpp::Light light("ambient");
  light.commit();

  world.setParam("light", ospray::cpp::CopiedData(light));
  world.commit();

  // create renderer, choose Scientific Visualization renderer
  ospray::cpp::Renderer renderer("scivis");

  // complete setup of renderer
  renderer.setParam("aoSamples", 1);
  renderer.setParam("backgroundColor", 1.0f); // white, transparent
  renderer.commit();

  // create and setup framebuffer
  ospray::cpp::FrameBuffer framebuffer(ctx->imgSize.x,
      ctx->imgSize.y,
      OSP_FB_SRGBA,
      OSP_FB_COLOR | OSP_FB_ACCUM | OSP_FB_VARIANCE);
  framebuffer.clear();

  ctx->camera = camera;
  ctx->world = world;
  ctx->renderer = renderer;
  ctx->framebuffer = framebuffer;
}

void buildScene2(context_t *ctx)
{
  // camera
  vec3f cam_pos{2.0f, -1.f, -4.f};
  vec3f cam_up{0.f, 1.f, 0.f};
  vec3f cam_view{-0.2f, 0.25f, 1.f};

  // triangle mesh data
  std::vector<vec3f> vertex = {vec3f(-2.0f, -2.0f, 2.0f),
      vec3f(-2.0f, 3.0f, 2.0f),
      vec3f(2.0f, -2.0f, 2.0f),
      vec3f(0.1f, -0.1f, 1.f)};
  std::vector<vec4f> color = {vec4f(0.0f, 0.1f, 0.8f, 1.0f),
      vec4f(0.8f, 0.8f, 0.0f, 1.0f),
      vec4f(0.8f, 0.8f, 0.0f, 1.0f),
      vec4f(0.9f, 0.1f, 0.0f, 1.0f)};
  std::vector<vec3ui> index = {vec3ui(0, 1, 2), vec3ui(1, 2, 3)};

  // create and setup camera
  ospray::cpp::Camera camera("perspective");
  camera.setParam("aspect", ctx->imgSize.x / (float)ctx->imgSize.y);
  camera.setParam("position", cam_pos);
  camera.setParam("direction", cam_view);
  camera.setParam("up", cam_up);
  camera.commit(); // commit each object to indicate modifications are done

  // create and setup model and mesh
  ospray::cpp::Geometry mesh("mesh");
  mesh.setParam("vertex.position", ospray::cpp::CopiedData(vertex));
  mesh.setParam("vertex.color", ospray::cpp::CopiedData(color));
  mesh.setParam("index", ospray::cpp::CopiedData(index));
  mesh.commit();

  // put the mesh into a model
  ospray::cpp::GeometricModel model(mesh);
  model.commit();

  // put the model into a group (collection of models)
  ospray::cpp::Group group;
  group.setParam("geometry", ospray::cpp::CopiedData(model));
  group.commit();

  // put the group into an instance (give the group a world transform)
  ospray::cpp::Instance instance(group);
  instance.commit();

  // put the instance in the world
  ospray::cpp::World world;
  world.setParam("instance", ospray::cpp::CopiedData(instance));

  // create and setup light for Ambient Occlusion
  ospray::cpp::Light light("ambient");
  light.commit();

  world.setParam("light", ospray::cpp::CopiedData(light));
  world.commit();

  // create renderer, choose Scientific Visualization renderer
  ospray::cpp::Renderer renderer("scivis");

  // complete setup of renderer
  renderer.setParam("aoSamples", 4);
  renderer.setParam("backgroundColor", 0.2f); // gray, transparent
  renderer.commit();

  // create and setup framebuffer
  ospray::cpp::FrameBuffer framebuffer(ctx->imgSize.x,
      ctx->imgSize.y,
      OSP_FB_SRGBA,
      OSP_FB_COLOR | OSP_FB_ACCUM | OSP_FB_VARIANCE);
  framebuffer.clear();

  ctx->camera = camera;
  ctx->world = world;
  ctx->renderer = renderer;
  ctx->framebuffer = framebuffer;
}

void buildSceneFile(context_t *ctx, std::string file)
{
  using json = vidi::json;

  // read a JSON file
  json jsroot;
  {
    std::ifstream jsroot_stream(file);
    jsroot_stream >> jsroot;
  }

  const json &jsview = jsroot.contains("view") ? jsroot["view"]
      : jsroot.contains("scene")               ? jsroot["scene"]
                                               : json(nullptr);
  if (jsview.is_null()) {
    throw std::runtime_error("cannot find 'view' specification in JSON");
  }

  assert(jsroot["dataSource"].size() == 1);

  ospray::cpp::Volume volume;
  std::shared_ptr<char[]> data;

  const json &jsdata = jsroot["dataSource"][0];
  {
    auto file = get_from_json<std::string>(jsdata["fileName"]);
    auto dims = get_from_json<vec3i>(jsdata["dimensions"]);
    auto type = get_from_json<OSPDataType>(jsdata["type"]);
    std::cout << "[debug] " << file << std::endl;
    std::cout << "[debug] " << dims << std::endl;
    std::cout << "[debug] " << type << std::endl;

    vec3f spacing(1.f);
    if (jsdata.contains("gridSpacing"))
      spacing = get_from_json<vec3f>(jsdata["gridSpacing"]);

    std::tie(volume, data) = RegularVolumeReader(file, type, dims, 0, false);
  }

  vec2f range;
  const json &jsvolume = jsview["volume"];
  {
    if (jsvolume.contains("scalarMappingRange")) {
      range.x = jsvolume["scalarMappingRange"]["minimum"].get<float>();
      range.y = jsvolume["scalarMappingRange"]["maximum"].get<float>();
    } else if (jsvolume.contains("scalarDomain")) {
      range.x = jsvolume["scalarDomain"]["minimum"].get<float>();
      range.y = jsvolume["scalarDomain"]["maximum"].get<float>();
    } else {
      throw std::runtime_error(
          "cannot read 'scalarMappingRange' or 'scalarDomain' in JSON");
    }
    std::cout << "[debug] range " << range << std::endl;
  }

  std::vector<float> opacities;
  std::vector<vec3f> colors;
  const json &jstfn = jsvolume["transferFunction"];
  {
    using namespace vidi;
    auto transferfunction = get_from_json<TransferFunction>(jstfn);
    auto *rgba = transferfunction.data();
    for (int i = transferfunction.size() - 1; i >= 0; --i) {
      std::cout << i << " " << rgba[i] << std::endl;
      opacities.push_back(rgba[i].w / 32.f);
      colors.push_back(vec3f(rgba[i].x, rgba[i].y, rgba[i].z));
    }
  }

  ospray::cpp::TransferFunction tfn("piecewiseLinear");
  tfn.setParam("valueRange", range);
  tfn.setParam("color", ospray::cpp::CopiedData(colors));
  tfn.setParam("opacity", ospray::cpp::CopiedData(opacities));
  tfn.commit();

  ospray::cpp::VolumetricModel model(volume);
  model.setParam("transferFunction", tfn);
  model.commit();

  ospray::cpp::Group group;
  group.setParam("volume", ospray::cpp::CopiedData(model));
  group.commit();

  // put the group into an instance (give the group a world transform)
  ospray::cpp::Instance instance(group);
  instance.commit();

  // // Skip clipping if not enabled
  // if (withClipping) {
  //   // Create clipping sphere
  //   cpp::Geometry sphereGeometry("sphere");
  //   std::vector<vec3f> position = {vec3f(-.5f, .2f, -.5f)};
  //   sphereGeometry.setParam("sphere.position", cpp::CopiedData(position));
  //   sphereGeometry.setParam("radius", 1.2f);
  //   sphereGeometry.commit();
  //
  //   cpp::GeometricModel model(sphereGeometry);
  //   model.commit();
  //
  //   cpp::Group group;
  //   group.setParam("clippingGeometry", cpp::CopiedData(model));
  //   group.commit();
  //
  //   cpp::Instance inst(group);
  //   inst.commit();
  //   instances.push_back(inst);
  // }

  // put the instance in the world
  ospray::cpp::World world;
  world.setParam("instance", ospray::cpp::CopiedData(instance));

  // create and setup light for Ambient Occlusion

  // std::vector<ospray::cpp::Light> lights;

  // ospray::cpp::Light quadLight("quad");
  // quadLight.setParam("position", vec3f(-4.0f, 3.0f, 1.0f));
  // quadLight.setParam("edge1", vec3f(0.f, 0.0f, -1.0f));
  // quadLight.setParam("edge2", vec3f(1.0f, 0.5, 0.0f));
  // quadLight.setParam("intensity", 50.0f);
  // quadLight.setParam("color", vec3f(2.6f, 2.5f, 2.3f));
  // quadLight.commit();

  // ospray::cpp::Light ambientLight("ambient");
  // ambientLight.setParam("intensity", 0.4f);
  // ambientLight.setParam("color", vec3f(1.f));
  // ambientLight.setParam("visible", false);
  // ambientLight.commit();

  // lights.push_back(quadLight);
  // lights.push_back(ambientLight);

  ospray::cpp::Light light("ambient");
  light.commit();

  world.setParam("light", ospray::cpp::CopiedData(light));
  world.commit();

  // create renderer, choose Scientific Visualization renderer
  ospray::cpp::Renderer renderer("scivis");

  // complete setup of renderer
  renderer.setParam("aoSamples", 4);
  renderer.setParam("backgroundColor", 0.f); // gray, transparent
  renderer.commit();

  // create and setup framebuffer
  ospray::cpp::FrameBuffer framebuffer(ctx->imgSize.x,
      ctx->imgSize.y,
      OSP_FB_SRGBA,
      OSP_FB_COLOR | OSP_FB_ACCUM | OSP_FB_VARIANCE);
  framebuffer.clear();

  // create and setup camera
  ospray::cpp::Camera camera;
  const json &jscamera = jsview["camera"];
  {
    auto cam_focus = get_from_json<vec3f>(jscamera["center"]);
    auto cam_pos = get_from_json<vec3f>(jscamera["eye"]) - 100.f;
    auto cam_up = get_from_json<vec3f>(jscamera["up"]);
    auto cam_view = rkcommon::math::normalize(cam_focus - cam_pos);

    std::cout << "[debug] cam_view " << cam_view << std::endl;
    std::cout << "[debug] cam_pos " << cam_pos << std::endl;
    std::cout << "[debug] cam_up " << cam_up << std::endl;

    auto fovy = get_from_json<float>(jscamera["fovy"]);
    auto znear = get_from_json<float>(jscamera["zNear"]);

    camera = ospray::cpp::Camera("perspective");
    camera.setParam("aspect", ctx->imgSize.x / (float)ctx->imgSize.y);
    camera.setParam("position", cam_pos);
    camera.setParam("direction", cam_view);
    camera.setParam("up", cam_up);
    camera.setParam("fovy", fovy);
    camera.setParam("nearClip", znear);
    camera.commit(); // commit each object to indicate modifications are done
  }

  ctx->camera = camera;
  ctx->world = world;
  ctx->renderer = renderer;
  ctx->framebuffer = framebuffer;

  ctx->userdata.push_back(data);
}
