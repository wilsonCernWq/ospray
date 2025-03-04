// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ObjectHandle.h"
#include <map>
#include <stack>

namespace ospray {

static std::map<int64, memory::RefCount *> objectByHandle;
static std::stack<int64> freedHandles;

//! next unassigned ID on this node
/*! we start numbering with 1 to make sure that "0:0" is an
  invalid handle (so we can typecast between (64-bit) handles
  and (64-bit)OSPWhatEver pointers */
static int32 nextFreeLocalID = 1;

void ObjectHandle::free()
{
  freedHandles.push((int64) * this);
}

ObjectHandle::ObjectHandle() : i64(NULL_HANDLE) {}

ObjectHandle::ObjectHandle(int64 i) : i64(i) {}

ObjectHandle::ObjectHandle(const ObjectHandle &other) : i64(other.i64) {}

ObjectHandle &ObjectHandle::operator=(const ObjectHandle &other)
{
  i64 = other.i64;
  return *this;
}

/*! define the given handle to refer to given object */
void ObjectHandle::assign(const ObjectHandle &handle, memory::RefCount *object)
{
  objectByHandle[handle] = object;
}

void ObjectHandle::assign(memory::RefCount *object) const
{
  objectByHandle[i64] = object;
}

void ObjectHandle::freeObject() const
{
  auto it = objectByHandle.find(i64);
  if (it != objectByHandle.end()) {
    // let internal device handle this -> it->second->refDec();
    objectByHandle.erase(it);
  }
}

int32 ObjectHandle::ownerRank() const
{
  return i32.owner;
}

int32 ObjectHandle::objID() const
{
  return i32.ID;
}

ospray::ObjectHandle::operator int64() const
{
  return i64;
}

bool ObjectHandle::defined() const
{
  return objectByHandle.find(i64) != objectByHandle.end();
}

ObjectHandle ObjectHandle::allocateLocalHandle()
{
  ObjectHandle handle;
  if (freedHandles.empty()) {
    handle.i32.ID = nextFreeLocalID++;
    handle.i32.owner = 0;
  } else {
    handle.i64 = freedHandles.top();
    freedHandles.pop();
  }
  return handle;
}

memory::RefCount *ObjectHandle::lookup() const
{
  if (i64 == 0)
    return nullptr;

  auto it = objectByHandle.find(i64);
  if (it == objectByHandle.end()) {
#ifndef NDEBUG
    // iw - made this into a warning only; the original code had
    // this throw an actual exceptoin, but that may be overkill
    std::cout << "#osp: WARNING: ospray is trying to look up object handle "
            + std::to_string(i64) + " that isn't defined!"
              << std::endl;
#endif
    return nullptr;
  }

  return it->second;
}

ObjectHandle ObjectHandle::lookup(memory::RefCount *object)
{
  for (auto it = objectByHandle.begin(); it != objectByHandle.end(); it++) {
    if (it->second == object)
      return (ObjectHandle(it->first));
  }

  return (nullHandle);
}

const ObjectHandle nullHandle(0);

} // namespace ospray
