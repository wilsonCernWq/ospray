// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// ospray
#include "MultiDevice.h"
#include "ISPCDevice.h"
#include "camera/registration.h"
#include "fb/LocalFB.h"
#include "fb/registration.h"
#include "geometry/registration.h"
#include "lights/registration.h"
#include "render/LoadBalancer.h"
#include "render/RenderTask.h"
#include "render/registration.h"
#include "rkcommon/tasking/parallel_for.h"
#include "rkcommon/tasking/tasking_system_init.h"
#include "rkcommon/utility/CodeTimer.h"
#include "rkcommon/utility/getEnvVar.h"
#include "rkcommon/utility/multidim_index_sequence.h"
#include "texture/registration.h"
#include "volume/transferFunction/registration.h"

namespace ospray {
namespace api {

/////////////////////////////////////////////////////////////////////////
// ManagedObject Implementation /////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
void MultiDevice::commit()
{
  Device::commit();

  if (subdevices.empty()) {
    auto OSPRAY_NUM_SUBDEVICES =
        utility::getEnvVar<int>("OSPRAY_NUM_SUBDEVICES");
    int numSubdevices =
        OSPRAY_NUM_SUBDEVICES.value_or(getParam("numSubdevices", 1));

    std::cout << "# of subdevices =" << numSubdevices << std::endl;

    std::vector<std::shared_ptr<TiledLoadBalancer>> subdeviceLoadBalancers;
    for (int i = 0; i < numSubdevices; ++i) {
      auto d = make_unique<ISPCDevice>();
      d->commit();
      subdevices.emplace_back(std::move(d));
      subdeviceLoadBalancers.push_back(subdevices.back()->loadBalancer);
    }
    loadBalancer = make_unique<MultiDeviceLoadBalancer>(subdeviceLoadBalancers);
  }

  // ISPCDevice::commit will init the tasking system but here we can reset
  // it to the global desired number of threads
  tasking::initTaskingSystem(numThreads, true);
}

/////////////////////////////////////////////////////////////////////////
// Device Implementation ////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int MultiDevice::loadModule(const char *name)
{
  OSPError err = OSP_NO_ERROR;
  for (auto &d : subdevices) {
    auto e = d->loadModule(name);
    if (e != OSP_NO_ERROR) {
      err = (OSPError)e;
    }
  }
  return err;
}

// OSPRay Data Arrays ///////////////////////////////////////////////////

OSPData MultiDevice::newSharedData(const void *sharedData,
    OSPDataType type,
    const vec3ul &numItems,
    const vec3l &byteStride)
{
  MultiDeviceObject *o = new MultiDeviceObject;
  // Data arrays of OSPRay objects need to populate the corresponding subdevice
  // data arrays with the objects for that subdevice
  // TODO: This also means we need to keep track of the original data
  // array pointer because we're going to lose that pointer here
  if (type & OSP_OBJECT) {
    for (size_t i = 0; i < subdevices.size(); ++i) {
      o->objects.push_back((OSPObject) new Data(type, numItems));
    }

    // A little lazy here, but using the Data object to just give me a view
    // + the index sequence iterator to use to step over the stride
    Data *multiData = new Data(sharedData, type, numItems, byteStride);
    index_sequence_3D seq(numItems);
    for (auto idx : seq) {
      MultiDeviceObject *mobj = *(MultiDeviceObject **)multiData->data(idx);
      retain((OSPObject)mobj);

      // Copy the subdevice object handles to the data arrays for each subdevice
      for (size_t i = 0; i < subdevices.size(); ++i) {
        Data *subdeviceData = (Data *)o->objects[i];
        std::memcpy(
            subdeviceData->data(idx), &mobj->objects[i], sizeof(OSPObject));
      }
    }
    multiData->refDec();
  } else {
    for (auto &d : subdevices) {
      o->objects.push_back(
          d->newSharedData(sharedData, type, numItems, byteStride));
    }
  }
  return (OSPData)o;
}

OSPData MultiDevice::newData(OSPDataType type, const vec3ul &numItems)
{
  MultiDeviceObject *o = new MultiDeviceObject;
  for (auto &d : subdevices) {
    o->objects.push_back(d->newData(type, numItems));
  }
  return (OSPData)o;
}

void MultiDevice::copyData(
    const OSPData source, OSPData destination, const vec3ul &destinationIndex)
{
  const MultiDeviceObject *srcs = (const MultiDeviceObject *)source;
  MultiDeviceObject *dsts = (MultiDeviceObject *)destination;
  for (size_t i = 0; i < subdevices.size(); ++i) {
    subdevices[i]->copyData(
        (OSPData)srcs->objects[i], (OSPData)dsts->objects[i], destinationIndex);
  }
}

// Renderable Objects ///////////////////////////////////////////////////

OSPLight MultiDevice::newLight(const char *type)
{
  MultiDeviceObject *o = new MultiDeviceObject;
  for (auto &d : subdevices) {
    o->objects.push_back(d->newLight(type));
  }
  return (OSPLight)o;
}

OSPCamera MultiDevice::newCamera(const char *type)
{
  MultiDeviceObject *o = new MultiDeviceObject;
  for (auto &d : subdevices) {
    o->objects.push_back(d->newCamera(type));
  }
  return (OSPCamera)o;
}

OSPGeometry MultiDevice::newGeometry(const char *type)
{
  MultiDeviceObject *o = new MultiDeviceObject;
  for (auto &d : subdevices) {
    o->objects.push_back(d->newGeometry(type));
  }
  return (OSPGeometry)o;
}

OSPVolume MultiDevice::newVolume(const char *type)
{
  MultiDeviceObject *o = new MultiDeviceObject;
  for (auto &d : subdevices) {
    o->objects.push_back(d->newVolume(type));
  }
  return (OSPVolume)o;
}

OSPGeometricModel MultiDevice::newGeometricModel(OSPGeometry geom)
{
  MultiDeviceObject *g = (MultiDeviceObject *)geom;
  MultiDeviceObject *o = new MultiDeviceObject;
  for (size_t i = 0; i < subdevices.size(); ++i) {
    o->objects.push_back(
        subdevices[i]->newGeometricModel((OSPGeometry)g->objects[i]));
  }
  return (OSPGeometricModel)o;
}
OSPVolumetricModel MultiDevice::newVolumetricModel(OSPVolume volume)
{
  MultiDeviceObject *v = (MultiDeviceObject *)volume;
  MultiDeviceObject *o = new MultiDeviceObject;
  for (size_t i = 0; i < subdevices.size(); ++i) {
    o->objects.push_back(
        subdevices[i]->newVolumetricModel((OSPVolume)v->objects[i]));
  }
  return (OSPVolumetricModel)o;
}

// Model Meta-Data //////////////////////////////////////////////////////

OSPMaterial MultiDevice::newMaterial(
    const char *renderer_type, const char *material_type)
{
  MultiDeviceObject *o = new MultiDeviceObject;
  for (auto &d : subdevices) {
    o->objects.push_back(d->newMaterial(renderer_type, material_type));
  }
  return (OSPMaterial)o;
}

OSPTransferFunction MultiDevice::newTransferFunction(const char *type)
{
  MultiDeviceObject *o = new MultiDeviceObject;
  for (auto &d : subdevices) {
    o->objects.push_back(d->newTransferFunction(type));
  }
  return (OSPTransferFunction)o;
}

OSPTexture MultiDevice::newTexture(const char *type)
{
  MultiDeviceObject *o = new MultiDeviceObject;
  for (auto &d : subdevices) {
    o->objects.push_back(d->newTexture(type));
  }
  return (OSPTexture)o;
}

// Instancing ///////////////////////////////////////////////////////////

OSPGroup MultiDevice::newGroup()
{
  MultiDeviceObject *o = new MultiDeviceObject;
  for (auto &d : subdevices) {
    o->objects.push_back(d->newGroup());
  }
  return (OSPGroup)o;
}

OSPInstance MultiDevice::newInstance(OSPGroup group)
{
  MultiDeviceObject *g = (MultiDeviceObject *)group;
  MultiDeviceObject *o = new MultiDeviceObject;
  for (size_t i = 0; i < subdevices.size(); ++i) {
    o->objects.push_back(subdevices[i]->newInstance((OSPGroup)g->objects[i]));
  }
  return (OSPInstance)o;
}

// Top-level Worlds /////////////////////////////////////////////////////

OSPWorld MultiDevice::newWorld()
{
  MultiDeviceObject *o = new MultiDeviceObject;
  for (auto &d : subdevices) {
    o->objects.push_back(d->newWorld());
  }
  return (OSPWorld)o;
}

box3f MultiDevice::getBounds(OSPObject obj)
{
  MultiDeviceObject *o = (MultiDeviceObject *)obj;
  // Everything is replicated across the subdevices, so we
  // can just query the bounds from one of them
  return subdevices[0]->getBounds(o->objects[0]);
}

// Object + Parameter Lifetime Management ///////////////////////////////

void MultiDevice::setObjectParam(
    OSPObject object, const char *name, OSPDataType type, const void *mem)
{
  // If the type is an OSPObject, we need to find the per-subdevice objects
  // and set them appropriately
  MultiDeviceObject *o = (MultiDeviceObject *)object;
  if (type & OSP_OBJECT) {
    MultiDeviceObject *p = *(MultiDeviceObject **)mem;
    for (size_t i = 0; i < subdevices.size(); ++i) {
      // TODO: I think it should be the address if I remember right
      subdevices[i]->setObjectParam(o->objects[i], name, type, &p->objects[i]);
    }
  } else {
    for (size_t i = 0; i < subdevices.size(); ++i) {
      subdevices[i]->setObjectParam(o->objects[i], name, type, mem);
    }
  }
}

void MultiDevice::removeObjectParam(OSPObject object, const char *name)
{
  MultiDeviceObject *o = (MultiDeviceObject *)object;
  for (size_t i = 0; i < subdevices.size(); ++i) {
    subdevices[i]->removeObjectParam(o->objects[i], name);
  }
}

void MultiDevice::commit(OSPObject object)
{
  // TODO: Needs to handle updating subdevice shared data for
  // subdevice object data arrays. While non-object arrays will
  // work fine on commit, arrays of objects need the handles translated
  // down to the subdevice specific handle
  // For this probably do want a multidevicedata that can keep a pointer
  // to the original data we got and can translate it back down on commit
  MultiDeviceObject *o = (MultiDeviceObject *)object;
  for (size_t i = 0; i < subdevices.size(); ++i) {
    subdevices[i]->commit(o->objects[i]);
  }
}
void MultiDevice::release(OSPObject object)
{
  memory::RefCount *o = (memory::RefCount *)object;
  MultiDeviceObject *mo = dynamic_cast<MultiDeviceObject *>(o);
  if (mo) {
    for (size_t i = 0; i < mo->objects.size(); ++i) {
      subdevices[i]->release(mo->objects[i]);
    }
  }
  o->refDec();
}
void MultiDevice::retain(OSPObject object)
{
  memory::RefCount *o = (memory::RefCount *)object;
  MultiDeviceObject *mo = dynamic_cast<MultiDeviceObject *>(o);
  if (mo) {
    for (size_t i = 0; i < mo->objects.size(); ++i) {
      subdevices[i]->retain(mo->objects[i]);
    }
  }
  o->refInc();
}

// FrameBuffer Manipulation /////////////////////////////////////////////

OSPFrameBuffer MultiDevice::frameBufferCreate(
    const vec2i &size, const OSPFrameBufferFormat mode, const uint32 channels)
{
  // TODO: For the CPU only multidevice development part, they will
  // all share the same CPU framebuffer, which can be created once
  // and managed by the multidevice
  // TODO: framebuffers will need special treatment in the rest of the API calls
  // or we can pretend it's a multideviceobject but where all objects reference
  // the same underlying object.
  // The latter is used right now, and we treat it as a MultiDeviceObject but
  // just have all subdevices reference the same thing
  // This does mean that things like setparam calls are called repeatedly
  // on the same object, but that should be ok

  FrameBuffer::ColorBufferFormat colorBufferFormat = mode;
  FrameBuffer *fb = new LocalFrameBuffer(size, colorBufferFormat, channels);
  MultiDeviceObject *o = new MultiDeviceObject();
  for (size_t i = 0; i < subdevices.size(); ++i) {
    o->objects.push_back((OSPFrameBuffer)fb);
  }
  return (OSPFrameBuffer)o;
}

OSPImageOperation MultiDevice::newImageOp(const char *type)
{
  // Same note for image ops as for framebuffers in terms of how they are
  // treated as shared. Eventually we would have per hardware device ones though
  // for cpu/gpus
  auto *op = ImageOp::createInstance(type);
  MultiDeviceObject *o = new MultiDeviceObject();
  for (size_t i = 0; i < subdevices.size(); ++i) {
    o->objects.push_back((OSPImageOperation)op);
  }
  return (OSPImageOperation)o;
}

const void *MultiDevice::frameBufferMap(
    OSPFrameBuffer _fb, const OSPFrameBufferChannel channel)
{
  MultiDeviceObject *o = (MultiDeviceObject *)_fb;
  LocalFrameBuffer *fb = (LocalFrameBuffer *)o->objects[0];
  return fb->mapBuffer(channel);
}

void MultiDevice::frameBufferUnmap(const void *mapped, OSPFrameBuffer _fb)
{
  MultiDeviceObject *o = (MultiDeviceObject *)_fb;
  LocalFrameBuffer *fb = (LocalFrameBuffer *)o->objects[0];
  fb->unmap(mapped);
}

float MultiDevice::getVariance(OSPFrameBuffer _fb)
{
  MultiDeviceObject *o = (MultiDeviceObject *)_fb;
  LocalFrameBuffer *fb = (LocalFrameBuffer *)o->objects[0];
  return fb->getVariance();
}

void MultiDevice::resetAccumulation(OSPFrameBuffer _fb)
{
  MultiDeviceObject *o = (MultiDeviceObject *)_fb;
  LocalFrameBuffer *fb = (LocalFrameBuffer *)o->objects[0];
  fb->clear();
}

// Frame Rendering //////////////////////////////////////////////////////

OSPRenderer MultiDevice::newRenderer(const char *type)
{
  MultiDeviceObject *o = new MultiDeviceObject;
  for (auto &d : subdevices) {
    o->objects.push_back(d->newRenderer(type));
  }
  return (OSPRenderer)o;
}

OSPFuture MultiDevice::renderFrame(OSPFrameBuffer _fb,
    OSPRenderer _renderer,
    OSPCamera _camera,
    OSPWorld _world)
{
  MultiDeviceObject *multiFb = (MultiDeviceObject *)_fb;
  FrameBuffer *fb = (FrameBuffer *)multiFb->objects[0];
  fb->setCompletedEvent(OSP_NONE_FINISHED);

  MultiDeviceObject *multiRenderer = (MultiDeviceObject *)_renderer;
  MultiDeviceObject *multiCamera = (MultiDeviceObject *)_camera;
  MultiDeviceObject *multiWorld = (MultiDeviceObject *)_world;
  fb->refInc();
  retain((OSPObject)multiRenderer);
  retain((OSPObject)multiCamera);
  retain((OSPObject)multiWorld);

  auto *f = new RenderTask(fb, [=]() {
    utility::CodeTimer timer;
    timer.start();
    loadBalancer->renderFrame(fb, multiRenderer, multiCamera, multiWorld);
    timer.stop();

    fb->refDec();
    release((OSPObject)multiRenderer);
    release((OSPObject)multiCamera);
    release((OSPObject)multiWorld);

    return timer.seconds();
  });

  return (OSPFuture)f;
}

int MultiDevice::isReady(OSPFuture _task, OSPSyncEvent event)
{
  auto *task = (Future *)_task;
  return task->isFinished(event);
}

void MultiDevice::wait(OSPFuture _task, OSPSyncEvent event)
{
  auto *task = (Future *)_task;
  task->wait(event);
}

void MultiDevice::cancel(OSPFuture _task)
{
  auto *task = (Future *)_task;
  return task->cancel();
}

float MultiDevice::getProgress(OSPFuture _task)
{
  auto *task = (Future *)_task;
  return task->getProgress();
}

float MultiDevice::getTaskDuration(OSPFuture _task)
{
  auto *task = (Future *)_task;
  return task->getTaskDuration();
}

OSPPickResult MultiDevice::pick(OSPFrameBuffer _fb,
    OSPRenderer _renderer,
    OSPCamera _camera,
    OSPWorld _world,
    const vec2f &screenPos)
{
  MultiDeviceObject *multiFb = (MultiDeviceObject *)_fb;
  FrameBuffer *fb = (FrameBuffer *)multiFb->objects[0];

  MultiDeviceObject *multiRenderer = (MultiDeviceObject *)_renderer;
  MultiDeviceObject *multiCamera = (MultiDeviceObject *)_camera;
  MultiDeviceObject *multiWorld = (MultiDeviceObject *)_world;

  // Data in the multidevice is all replicated, so we just run the pick on the
  // first subdevice
  return subdevices[0]->pick((OSPFrameBuffer)fb,
      (OSPRenderer)multiRenderer->objects[0],
      (OSPCamera)multiCamera->objects[0],
      (OSPWorld)multiWorld->objects[0],
      screenPos);
}

#if 0
extern "C" OSPError OSPRAY_DLLEXPORT ospray_module_init_multi(
    int16_t versionMajor, int16_t versionMinor, int16_t /*versionPatch*/)
{
  auto status = moduleVersionCheck(versionMajor, versionMinor);

  if (status == OSP_NO_ERROR) {
    // Run the ISPC module's initialization function as well to register local
    // types
    status = ospLoadModule("ispc");
  }

  if (status == OSP_NO_ERROR) {
    Device::registerType<MultiDevice>("multi");
  }

  return status;
}
#endif

} // namespace api
} // namespace ospray
