// Copyright 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Builder.h"
#include "ospray_testing.h"
#include "rkcommon/math/AffineSpace.h"
#include "rkcommon/tasking/parallel_for.h"

#include "openvkl/openvkl.h"
#include "openvkl/vdb.h"

#if defined(__cplusplus)
#if defined(_WIN32)
#include <windows.h>
#else // Platform: Unix
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#endif

using namespace rkcommon::math;

namespace ospray {
namespace testing {

// ----------------------------------------------------------------------------
// File I/O
// ----------------------------------------------------------------------------
#if defined(_WIN32) // Create a string with last error message
inline void PrintLastError(const std::string &msg)
{
  DWORD error = GetLastError();
  if (error) {
    LPVOID lpMsgBuf;
    DWORD bufLen = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0,
        NULL);
    if (bufLen) {
      LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
      std::string result(lpMsgStr, lpMsgStr + bufLen);
      LocalFree(lpMsgBuf);
      std::cerr << msg << ": " << result << std::endl;
    }
  }
}
#endif defined(_WIN32)

struct FileMap
{
  enum IO_TYPE
  {
    BINARY_WRITE,
    BINARY_READ
  } type;
  size_t p = 0;
  size_t file_size = 0;
#if defined(_WIN32)
  HANDLE hFile = INVALID_HANDLE_VALUE;
  // HANDLE hMap  = INVALID_HANDLE_VALUE;
  // char *map    = NULL;
  static const DWORD stream_buffer_size = 512 * 1024;
  DWORD stream_p = 0;
  char stream_buffer[stream_buffer_size];

  // size_t refresh_buffer()
  // {
  //   const size_t avail = std::min(file_size - p, size_t(stream_buffer_size));
  //   if (stream_p >= avail) {
  //     stream_p = 0;
  //     DWORD bytes_read;
  //     if (!ReadFile(
  //             hFile, stream_buffer, stream_buffer_size, &bytes_read, NULL)) {
  //       PrintLastError("failed to load streaming buffer");
  //     }
  //     if (bytes_read < stream_buffer_size) {
  //       std::cout << "strange bytes_read " << bytes_read << std::endl;
  //     }
  //     p += avail;
  //   }
  //   return avail;
  // }
#else // Platform: Unix
  char *map = (char *)MAP_FAILED;
  int fd = -1;
#endif // defined(_WIN32)
};

inline FileMap filemap_read_create(const std::string &filename)
{
  FileMap ret;
  ret.type = FileMap::BINARY_READ;

#if defined(_WIN32)

  ret.hFile = CreateFile(filename.c_str(),
      FILE_ATTRIBUTE_READONLY,
      0,
      NULL,
      OPEN_EXISTING,
      // FILE_FLAG_SEQUENTIAL_SCAN,
      FILE_FLAG_NO_BUFFERING,
      NULL);

  if (ret.hFile == INVALID_HANDLE_VALUE) {
    PrintLastError("failed to open file " + filename);
    throw std::runtime_error("termination caused by I/O errors");
  }

  LARGE_INTEGER file_size;
  GetFileSizeEx(ret.hFile, &file_size);
  if (file_size.QuadPart == 0) {
    CloseHandle(ret.hFile);
    throw std::runtime_error("cannot map 0 size file");
  }
  ret.file_size = file_size.QuadPart;

  // Now map the file

  // ret.hMap =
  //     CreateFileMapping(ret.hFile, nullptr, PAGE_READONLY, 0, 0,
  //     nullptr);
  // if (ret.hMap == INVALID_HANDLE_VALUE) {
  //   CloseHandle(ret.hFile);
  //   PrintLastError("failed to map file " + filename);
  //   throw std::runtime_error("termination caused by I/O errors");
  // }

  // ret.map =
  //     (char *)MapViewOfFile(ret.hMap, FILE_MAP_READ, 0, 0,
  //     ret.file_size);
  // if (!ret.map) {
  //   CloseHandle(ret.hMap);
  //   CloseHandle(ret.hFile);
  //   PrintLastError("failed to map view of file " + filename);
  //   throw std::runtime_error("termination caused by I/O errors");
  // }

#else // Platform: Unix

  ret.fd = open(filename.c_str(), O_RDONLY, (mode_t)0600);

  if (ret.fd == -1) {
    perror("error opening file for writing");
    throw std::runtime_error("termination caused by I/O errors");
  }

  struct stat fileInfo = {0};

  if (fstat(ret.fd, &fileInfo) == -1) {
    close(ret.fd);
    perror("error getting the file size");
    throw std::runtime_error("termination caused by I/O errors");
  }

  if (fileInfo.st_size == 0) {
    close(ret.fd);
    throw std::runtime_error("error: file is empty, nothing to do");
  }

  printf("file size is %ji\n", (intmax_t)fileInfo.st_size);

  ret.map = (char *)mmap(0, fileInfo.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (ret.map == MAP_FAILED) {
    close(ret.fd);
    perror("error mmapping the file");
    throw std::runtime_error("termination caused by I/O errors");
  }

  ret.file_size = fileInfo.st_size;

#endif // defined(_WIN32)

  return ret;
}

inline void filemap_read(FileMap &file, void *data, const size_t bytes)
{
  assert(file.type == FileMap::BINARY_READ);
  assert(bytes <= file.file_size);

  printf("read %zu bytes\n", bytes);

  char *text = (char *)data;

  size_t bytes_completed = 0;

  while (bytes_completed < bytes) {
    // compute how many bypes to read in the next stream
    const size_t avail =
        std::min(file.file_size - file.p, size_t(file.stream_buffer_size));

    // of course the stream progress should be resetted
    if (file.stream_p >= avail) {
      file.stream_p = 0;
    }

    // how many bytes to copy from the buffer
    const size_t bytes_to_read =
        std::min(bytes - bytes_completed, size_t(avail - file.stream_p));

    // whether we can skup the copy in this round
    bool skip_copy = false;

    if (file.stream_p == 0) {
      // when the data needed equals to the data to be streamed, we can directly
      // use the destination as the stream buffer. therefore here we use a
      // pointer to indicate the buffer to use for streaming.

      char *buf = file.stream_buffer;

      if (bytes_to_read == avail) {
        buf = &text[bytes_completed];
        skip_copy = true;
      }

      // actually stream data from file to the `buffer`
      DWORD bytes_read;
      if (!ReadFile(
              file.hFile, buf, file.stream_buffer_size, &bytes_read, NULL)) {
        PrintLastError("failed to load streaming buffer");
      }
      if (bytes_read < file.stream_buffer_size) {
        std::cout << "strange bytes_read " << bytes_read << std::endl;
      }
      assert(bytes_read == avail);
      
      // always update the file progress
      file.p += bytes_read;
    }

    // copy data from the stream buffer to the destination because additional
    // bytes are streamed.
    if (!skip_copy) {
      printf("copied\n");
      for (size_t i = 0; i < bytes_to_read; i++)
        text[bytes_completed + i] = file.stream_buffer[file.stream_p + i];
    }

    file.stream_p += bytes_to_read;
    bytes_completed += bytes_to_read;
  }

  // for (size_t i = 0; i < bytes; i++) {
  //   text[i] = file.map[file.p + i];
  // }

  // rkcommon::tasking::parallel_for(
  //     bytes, [&](size_t i) { text[i] = file.map[file.p + i]; });
}

inline void filemap_close(FileMap &file)
{
#if defined(_WIN32)

  // UnmapViewOfFile(file.map);
  // CloseHandle(file.hMap);
  CloseHandle(file.hFile);

#else // Platform: Unix

  // Don't forget to free the mmapped memory
  if (munmap(file.map, file.file_size) == -1) {
    close(file.fd);
    perror("error un-mmapping the file");
    throw std::runtime_error("termination caused by I/O errors");
  }

  // Un-mmaping doesn't close the file, so we still need to do that.
  close(file.fd);

#endif // defined(_WIN32)
}

// ----------------------------------------------------------------------------
// Builder
// ----------------------------------------------------------------------------
struct HtgVolume : public detail::Builder
{
  HtgVolume() = default;
  ~HtgVolume() override = default;

  void commit() override;

  cpp::Group buildGroup() const override;
  cpp::World buildWorld() const override;

 private:
  float densityScale{1.f};
  float anisotropy{0.f};
  static constexpr uint32_t domainRes = 128;
};

// Inlined definitions ////////////////////////////////////////////////////////

void HtgVolume::commit()
{
  Builder::commit();

  densityScale = getParam<float>("densityScale", 20.f);
  anisotropy = getParam<float>("anisotropy", 0.f);
}

cpp::Group HtgVolume::buildGroup() const
{
  LARGE_INTEGER beginClock, endClock, cpuClockFreq;
  QueryPerformanceFrequency(&cpuClockFreq);

  // auto reader = filemap_read_create("bunny_cloud.htg");
  auto reader = filemap_read_create("C:/Datasets/wdas_cloud.htg");

  vec3f _actualBounds;
  float _extendBounds;
  uint64_t numofNodes;
  range1f vrange;

  filemap_read(reader, &numofNodes, sizeof(uint64_t));
  std::cout << "- " << numofNodes << std::endl;

  filemap_read(reader, &_actualBounds, sizeof(vec3f));
  std::cout << "- " << _actualBounds.x << " " << _actualBounds.y << " "
            << _actualBounds.z << " " << std::endl;

  filemap_read(reader, &_extendBounds, sizeof(float));
  std::cout << "- " << _extendBounds << std::endl;

  filemap_read(reader, &vrange.lower, sizeof(float));
  filemap_read(reader, &vrange.upper, sizeof(float));

  // TODO allocating space, will cause memory leak ...
  std::cout << "allocating spaces" << std::endl;
  const size_t dataSize = sizeof(uint64_t) * numofNodes;
  unsigned char *dataBuffer = new unsigned char[dataSize];

  // bench mark I/O
  {
    std::cout << "reading " << numofNodes << " nodes => "
              << dataSize / 1024.f / 1024.f << " MB" << std::endl;
    QueryPerformanceCounter(&beginClock);
    filemap_read(reader, dataBuffer, sizeof(uint64_t) * numofNodes);
    QueryPerformanceCounter(&endClock);
    double wallTime_ticks = double(endClock.QuadPart - beginClock.QuadPart);
    double wallTime_us =
        wallTime_ticks * 1000.0 * 1000.0 / double(cpuClockFreq.QuadPart);
    float wallTime_ms = wallTime_us / 1000.0f;
    float wallTime_s = wallTime_us / 1000.0f;
    float estBW_MB_s = (float(dataSize) / wallTime_s) / 1000.0f / 1000.0f;
    std::cout << "read time: " << wallTime_ms << " ms" << std::endl;
    std::cout << "estimated bandwidth: " << estBW_MB_s << " MB/s" << std::endl;
  }

  filemap_close(reader);

  vec3f gridSpacing = (float)domainRes / _actualBounds;

  cpp::Data<true> ospData(dataBuffer, dataSize);

  cpp::Volume volume("htg");
  volume.setParam("filter", (int)OSP_VOLUME_FILTER_TRILINEAR);
  volume.setParam("gradientFilter", (int)OSP_VOLUME_FILTER_TRILINEAR);
  volume.setParam("gridOrigin", vec3f(0, 0, 0));
  volume.setParam("gridSpacing", gridSpacing);
  volume.setParam("actualBounds", _actualBounds);
  volume.setParam("nodeData", ospData);
  volume.commit();

  cpp::VolumetricModel model(volume);
  model.setParam("transferFunction", makeTransferFunction({0.f, 1.f}));
  model.setParam("densityScale", densityScale);
  model.setParam("anisotropy", anisotropy);
  model.commit();

  cpp::Group group;
  group.setParam("volume", cpp::CopiedData(model));
  group.commit();

  return group;
}

cpp::World HtgVolume::buildWorld() const
{
  auto group = buildGroup();
  cpp::Instance instance(group);
  rkcommon::math::AffineSpace3f xform(
      rkcommon::math::LinearSpace3f::scale(8.f / domainRes),
      vec3f(-4.f, 0, -4.f));
  instance.setParam("xfm", xform);
  instance.commit();

  std::vector<cpp::Instance> inst;
  inst.push_back(instance);

  if (addPlane)
    inst.push_back(makeGroundPlane(instance.getBounds<box3f>()));

  cpp::Light quadLight("quad");
  quadLight.setParam("position", vec3f(-4.f, 8.0f, 4.f));
  quadLight.setParam("edge1", vec3f(0.f, 0.0f, -8.0f));
  quadLight.setParam("edge2", vec3f(2.f, 1.0f, 0.0f));
  quadLight.setParam("intensity", 5.0f);
  quadLight.setParam("color", vec3f(2.8f, 2.2f, 1.9f));
  quadLight.commit();

  std::vector<cpp::Light> lightHandles;
  lightHandles.push_back(quadLight);

  cpp::World world;
  world.setParam("instance", cpp::CopiedData(inst));
  world.setParam("light", cpp::CopiedData(lightHandles));

  return world;
}

OSP_REGISTER_TESTING_BUILDER(HtgVolume, htg_volume);

} // namespace testing
} // namespace ospray