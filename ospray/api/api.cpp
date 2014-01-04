#include "ospray/include/ospray/ospray.h"
#include "ospray/render/renderer.h"
#include "ospray/camera/camera.h"
#include "localdevice.h"
#if OSPRAY_MPI
# include "mpidevice.h"
#endif

#if 1
# define LOG(a) if (logLevel > 2) std::cout << "#ospray: " << a << std::endl;
#else
# define LOG(a) /*ignore*/
#endif

/*! \file api.cpp implements the public ospray api functions by
    routing them to a respective \ref device */
namespace ospray {
  using std::endl;
  using std::cout;

#define ASSERT_DEVICE() if (ospray::api::Device::current == NULL)       \
    throw std::runtime_error("OSPRay not yet initialized "              \
                             "(most likely this means you tried to "    \
                             "call an ospray API function before "      \
                             "first calling ospInit())");

  
  extern "C" void ospInit(int *_ac, const char **_av) 
  {
    if (ospray::api::Device::current) 
      throw std::runtime_error("OSPRay error: device already exists "
                               "(did you call ospInit twice?)");

    // we're only supporting local rendering for now - network device
    // etc to come.
    if (std::string(_av[1]) == "--mpi") {
#if OSPRAY_MPI
      ospray::api::Device::current = new ospray::api::MPIDevice(_ac,_av);
#else
      throw std::runtime_error("OSPRay MPI support not compiled in");
#endif
    } else {
      ospray::api::Device::current = new ospray::api::LocalDevice(_ac,_av);
    }
  }

  /*! destroy a given frame buffer. 

   due to internal reference counting the framebuffer may or may not be deleted immeidately
  */
  extern "C" void ospFreeFrameBuffer(OSPFrameBuffer fb)
  {
    ASSERT_DEVICE();
    Assert(fb != NULL);
    std::cout << "warning: ospFreeFrameBuffer not yet implemented - ignoring (this means there is a memory hole!)" << std::endl;
  }

  extern "C" OSPFrameBuffer ospNewFrameBuffer(const osp::vec2i &size, 
                                              const OSPFrameBufferMode mode,
                                              const size_t swapChainDepth)
  {
    ASSERT_DEVICE();
    Assert(swapChainDepth > 0 && swapChainDepth < 4);
    return ospray::api::Device::current->frameBufferCreate(size,mode,swapChainDepth);
  }

  extern "C" const void *ospMapFrameBuffer(OSPFrameBuffer fb)
  {
    ASSERT_DEVICE();
    return ospray::api::Device::current->frameBufferMap(fb);
  }
  
  extern "C" void ospUnmapFrameBuffer(const void *mapped,
                                      OSPFrameBuffer fb)
  {
    ASSERT_DEVICE();
    Assert(mapped != NULL && "invalid mapped pointer in ospAddGeometry");
    ospray::api::Device::current->frameBufferUnmap(mapped,fb);
  }

  extern "C" OSPModel ospNewModel()
  {
    ASSERT_DEVICE();
    return ospray::api::Device::current->newModel();
  }

  extern "C" void ospAddGeometry(OSPModel model, OSPGeometry geometry)
  {
    ASSERT_DEVICE();
    Assert(model != NULL && "invalid model in ospAddGeometry");
    Assert(geometry != NULL && "invalid geometry in ospAddGeometry");
    return ospray::api::Device::current->addGeometry(model,geometry);
  }

  /*! create a newa data buffer, with optional init data and control flags */
  extern "C" OSPTriangleMesh ospNewTriangleMesh()
  {
    ASSERT_DEVICE();
    return ospray::api::Device::current->newTriangleMesh();
  }

  /*! create a new data buffer, with optional init data and control flags */
  extern "C" OSPData ospNewData(size_t nitems, OSPDataType format, void *init, int flags)
  {
    ASSERT_DEVICE();
    return ospray::api::Device::current->newData(nitems,format,init,flags);
  }

  /*! add a data array to another object */
  extern "C" void ospSetData(OSPObject object, const char *bufName, OSPData data)
  {
    ASSERT_DEVICE();
    LOG("ospSetData(...,\"" << bufName << "\",...)");
    return ospray::api::Device::current->setObject(object,bufName,(OSPObject)data);
  }

  /*! add a data array to another object */
  extern "C" void ospSetParam(OSPObject target, const char *bufName, OSPObject value)
  {
    ASSERT_DEVICE();
    LOG("ospSetData(...,\"" << bufName << "\",...)");
    return ospray::api::Device::current->setObject(target,bufName,value);
  }

  /*! \brief create a new renderer of given type */
  /*! \detailed return 'NULL' if that type is not known */
  extern "C" OSPRenderer ospNewRenderer(const char *type)
  {
    ASSERT_DEVICE();
    Assert(type != NULL && "invalid render type identifier in ospAddGeometry");
    LOG("ospNewRenderer(" << type << ")");
    OSPRenderer renderer = ospray::api::Device::current->newRenderer(type);
    if (logLevel > 0)
      if (renderer) 
        cout << "ospNewRenderer: " << ((ospray::Renderer*)renderer)->toString() << endl;
      else
        std::cerr << "#ospray: could not create renderer '" << type << "'" << std::endl;
    return renderer;
  }

  /*! \brief create a new camera of given type */
  /*! \detailed return 'NULL' if that type is not known */
  extern "C" OSPCamera ospNewCamera(const char *type)
  {
    ASSERT_DEVICE();
    Assert(type != NULL && "invalid render type identifier in ospAddGeometry");
    LOG("ospNewCamera(" << type << ")");
    OSPCamera camera = ospray::api::Device::current->newCamera(type);
    if (logLevel > 0)
      if (camera) 
        cout << "ospNewCamera: " << ((ospray::Camera*)camera)->toString() << endl;
      else
        std::cerr << "#ospray: could not create camera '" << type << "'" << std::endl;
    return camera;
  }

  /*! \brief call a renderer to render given model into given framebuffer */
  /*! \detailed model _may_ be empty (though most framebuffers will expect one!) */
  extern "C" void ospRenderFrame(OSPFrameBuffer fb, OSPRenderer renderer)
  {
    ASSERT_DEVICE();
    ospray::api::Device::current->renderFrame(fb,renderer);
  }

  /*! add a data array to another object */
  extern "C" void ospCommit(OSPObject object)
  {
    ASSERT_DEVICE();
    Assert(object && "invalid object handle to commit to");
    LOG("ospCommit(...)");
    return ospray::api::Device::current->commit(object);
  }

  extern "C" void ospSetf(OSPObject _object, const char *id, float x)
  {
    ASSERT_DEVICE();
    ospray::api::Device::current->setFloat(_object,id,x);
  }
  /*! add a data array to another object */
  extern "C" void ospSetVec3f(OSPObject _object, const char *id, const vec3f &v)
  {
    ASSERT_DEVICE();
    ospray::api::Device::current->setVec3f(_object,id,v);
  }
  /*! add a data array to another object */
  extern "C" void ospSet3f(OSPObject _object, const char *id, float x, float y, float z)
  {
    ASSERT_DEVICE();
    ospSetVec3f(_object,id,vec3f(x,y,z));
  }


}
