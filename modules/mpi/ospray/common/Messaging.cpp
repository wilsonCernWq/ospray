// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Messaging.h"
#include <mutex>
#include <unordered_map>
#include "rkcommon/memory/DeletedUniquePtr.h"

namespace ospray {
namespace mpi {
namespace messaging {

using namespace mpicommon;
using rkcommon::memory::DeletedUniquePtr;
using rkcommon::memory::make_deleted_unique;

// Internal maml message handler for all of OSPRay //////////////////////

struct ObjectMessageHandler : maml::MessageHandler
{
  virtual ~ObjectMessageHandler() override;

  void registerMessageListener(int handleObjID, maml::MessageHandler *listener);

  void removeMessageListener(int handleObjID);

  void incoming(const std::shared_ptr<Message> &message) override;

  // Data members //

  // The communicator used for object messages, to avoid conflicting
  // with other communication
  mpicommon::Group group;

 private:
  std::mutex objectListenersMutex;
  std::unordered_map<int, MessageHandler *> objectListeners;
};

// Inlined ObjectMessageHandler definitions /////////////////////////////

ObjectMessageHandler::~ObjectMessageHandler()
{
  if (group.comm != MPI_COMM_NULL) {
    MPI_Comm_free(&group.comm);
  }
}

inline void ObjectMessageHandler::registerMessageListener(
    int handleObjID, MessageHandler *listener)
{
  std::lock_guard<std::mutex> lock(objectListenersMutex);

  if (objectListeners.find(handleObjID) != objectListeners.end())
    postStatusMsg() << "WARNING: overwriting an existing listener!";

  objectListeners[handleObjID] = listener;
}

inline void ObjectMessageHandler::removeMessageListener(int handleObjID)
{
  std::lock_guard<std::mutex> lock(objectListenersMutex);
  objectListeners.erase(handleObjID);
}

inline void ObjectMessageHandler::incoming(
    const std::shared_ptr<Message> &message)
{
  std::lock_guard<std::mutex> lock(objectListenersMutex);
  auto obj = objectListeners.find(message->tag);
  if (obj != objectListeners.end()) {
    obj->second->incoming(message);
  } else {
    postStatusMsg() << "WARNING: No destination for incoming message "
                    << "with tag " << message->tag
                    << ", size = " << message->size;
  }
}

// Singleton instance (hidden) and helper creation function /////////////

static DeletedUniquePtr<ObjectMessageHandler> handler = nullptr;
static bool handlerValid = false;

// MessageHandler definitions ///////////////////////////////////////////

MessageHandler::MessageHandler(ObjectHandle handle) : myId(handle)
{
  registerMessageListener(myId, this);
}

MessageHandler::~MessageHandler()
{
  removeMessageListener(myId);
}

// ospray::mpi::messaging definitions ///////////////////////////////////

void init(mpicommon::Group parentGroup)
{
  if (handlerValid)
    throw std::runtime_error("Error: Object Messaging was already init");

  Group group = parentGroup.dup();
  handler = make_deleted_unique<ObjectMessageHandler>(
      [](ObjectMessageHandler *_handler) {
        handlerValid = false;
        delete _handler;
      });
  handler->group = group;

  maml::registerHandlerFor(group.comm, handler.get());
  handlerValid = true;
}

void shutdown()
{
  maml::shutdown();
  handler = nullptr;
}

void registerMessageListener(int handleObjID, MessageHandler *listener)
{
  if (!handlerValid)
    throw std::runtime_error("ObjectMessageHandler was not created!");

  handler->registerMessageListener(handleObjID, listener);
}

void removeMessageListener(int handleObjID)
{
  if (handlerValid)
    handler->removeMessageListener(handleObjID);
}

void enableAsyncMessaging()
{
  // TODO WILL: Supporting thread serialized is a pain and I don't
  // think it's necessary
  // maml::start();
}

void sendTo(int globalRank, ObjectHandle object, std::shared_ptr<Message> msg)
{
#ifdef DEBUG
  if (!handlerValid)
    throw std::runtime_error(
        "ObjMessageHandler must be created before"
        " sending object messages");
#endif
  msg->tag = object.objID();
  maml::sendTo(handler->group.comm, globalRank, msg);
}

bool asyncMessagingEnabled()
{
  return maml::isRunning();
}

void disableAsyncMessaging()
{
  // maml::stop();
}

} // namespace messaging
} // namespace mpi
} // namespace ospray
