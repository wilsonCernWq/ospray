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

// // ----------------------------------------------------------------------------
// // File I/O
// // ----------------------------------------------------------------------------
// inline void ThrowLastError(const std::string &msg)
// {
// #if defined(_WIN32) // Create a string with last error message
//   std::string result;
//   DWORD error = GetLastError();
//   if (error) {
//     LPVOID lpMsgBuf;
//     DWORD bufLen = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
//             | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
//         NULL,
//         error,
//         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
//         (LPTSTR)&lpMsgBuf,
//         0,
//         NULL);
//     if (bufLen) {
//       LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
//       result = lpMsgStr, lpMsgStr + bufLen;
//       LocalFree(lpMsgBuf);
//       // std::cerr << msg << ": " << result << std::endl;
//     }
//   }
//   throw std::runtime_error(msg + ": " + result);
// #else
//   perror(msg.c_str());
//   throw std::runtime_error("Termination caused by I/O errors");
// #endif // defined(_WIN32)
// }

// class FileRef
// {
//  protected:
//   enum Direction
//   {
//     READ,
//     WRITE
//   } direction;
// #if defined(_WIN32) // Platform: Windows
//   HANDLE hFile = INVALID_HANDLE_VALUE;
// #else // Platform: POSIX
//   int fd = -1;
// #endif // defined(_WIN32)
//   size_t file_p = 0;
//   size_t file_size = 0;

//  public:
//   virtual ~FileRef();
//   virtual void read(void *data, size_t bytes) = 0;
//   virtual void write(const void *data, size_t bytes) = 0;
// };

// typedef FileRef *FileMap;

// // File I/O using virtual memories (mmap)
// struct FileRef_VM : FileRef
// {
// #if defined(_WIN32)
//   HANDLE hMap = INVALID_HANDLE_VALUE;
//   char *map = NULL;
// #else // Platform: Unix
//   char *map = (char *)MAP_FAILED;
// #endif // defined(_WIN32)

//  public:
//   ~FileRef_VM();
//   /* reader */
//   FileRef_VM(const std::string &filename);
//   /* writer */
//   FileRef_VM(const std::string &filename, size_t requested_size);
//   void read(void *data, size_t bytes) override;
//   void write(const void *data, size_t bytes) override;
// };

// // #if defined(_WIN32)
// //     struct FileMapReader_ByFile : FileMap
// //     {
// //      public:
// //       FileMapReader_ByFile();
// //
// //      private:
// //       HANDLE hFile = INVALID_HANDLE_VALUE;
// //     };
// // #endif  // defined(_WIN32)

// #if defined(_WIN32)
// struct FileReader_ByByte : FileRef
// {
//   static const DWORD STREAM_BUFFER_SIZE = 512 * 1024;
//   char stream_buffer[STREAM_BUFFER_SIZE];
//   DWORD stream_p = 0;
//   HANDLE hFile = INVALID_HANDLE_VALUE;

//  public:
//   FileReader_ByByte(const std::string &filename);
//   void read(void *data, size_t bytes) override;
//   void write(const void *data, size_t bytes) override;
// };
// #endif // defined(_WIN32)

// // ----------------------------------------------------------------------------
// // Inline Implementations
// // ----------------------------------------------------------------------------

// inline FileRef::~FileRef()
// {
// #if defined(_WIN32) // Platform: Windows

//   FlushFileBuffers(this->hFile);
//   CloseHandle(this->hFile);

// #else // Platform: POSIX

//   // Un-mmaping doesn't close the file, so we still need to do that.
//   close(this->fd);

// #endif // defined(_WIN32)
// }

// inline FileRef_VM::~FileRef_VM()
// {
// #if defined(_WIN32) // Platform: Windows

//   UnmapViewOfFile(this->map);
//   CloseHandle(this->hMap);

// #else // Platform: POSIX

//   // Don't forget to free the mmapped memory
//   if (munmap(this->map, this->file_size) == -1) {
//     ThrowLastError("Error un-mmapping the file");
//   }

// #endif // defined(_WIN32)
// }

// inline FileRef_VM::FileRef_VM(const std::string &filename)
// {
//   direction = FileRef::READ;

// #if defined(_WIN32) // Platform: Windows

//   this->hFile = CreateFile(filename.c_str(),
//       FILE_ATTRIBUTE_READONLY,
//       FILE_SHARE_READ,
//       NULL,
//       OPEN_EXISTING,
//       FILE_FLAG_SEQUENTIAL_SCAN,
//       NULL);

//   if (this->hFile == INVALID_HANDLE_VALUE) {
//     ThrowLastError("failed to open file " + filename);
//   }

//   LARGE_INTEGER file_size;
//   GetFileSizeEx(this->hFile, &file_size);
//   if (file_size.QuadPart == 0) {
//     throw std::runtime_error("Cannot map 0 size file");
//   }

//   // Now map the file
//   this->hMap =
//       CreateFileMapping(this->hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
//   if (this->hMap == INVALID_HANDLE_VALUE) {
//     ThrowLastError("Failed to map file " + filename);
//   }

//   this->file_size = file_size.QuadPart;
//   this->map =
//       (char *)MapViewOfFile(this->hMap, FILE_MAP_READ, 0, 0, this->file_size);
//   if (!this->map) {
//     ThrowLastError("Failed to map view of file " + filename);
//   }

// #else // Platform: POSIX

//   this->fd = open(filename.c_str(), O_RDONLY, (mode_t)0600);

//   if (this->fd == -1) {
//     ThrowLastError("Error opening file for writing");
//   }

//   struct stat fileInfo = {0};

//   if (fstat(this->fd, &fileInfo) == -1) {
//     ThrowLastError("Error getting the file size");
//   }

//   if (fileInfo.st_size == 0) {
//     ThrowLastError("Error: File is empty, nothing to do");
//   }

//   printf("File size is %ji\n", (intmax_t)fileInfo.st_size);

//   this->map = (char *)mmap(0, fileInfo.st_size, PROT_READ, MAP_SHARED, fd, 0);
//   if (this->map == MAP_FAILED) {
//     ThrowLastError("Error mmapping the file");
//   }

//   this->file_size = fileInfo.st_size;

// #endif // defined(_WIN32)
// }

// inline FileRef_VM::FileRef_VM(
//     const std::string &filename, size_t requested_size)
// {
//   direction = FileRef::WRITE;

// #if defined(_WIN32) // Platform: Windows

//   this->hFile = CreateFile(filename.c_str(),
//       GENERIC_WRITE | GENERIC_READ,
//       FILE_SHARE_WRITE,
//       NULL,
//       CREATE_ALWAYS,
//       FILE_FLAG_WRITE_THROUGH,
//       NULL);

//   if (this->hFile == INVALID_HANDLE_VALUE) {
//     ThrowLastError("Failed to open file " + filename);
//   }

//   // Stretch the file size to the size of the (mapped) array of char
//   LARGE_INTEGER file_size;
//   file_size.QuadPart = requested_size;
//   if (SetFilePointerEx(this->hFile, file_size, NULL, FILE_BEGIN) == 0) {
//     ThrowLastError("Error calling lseek() to 'stretch' the file");
//   }

//   // Actually stretch the file
//   SetEndOfFile(this->hFile);

//   // Verify file size
//   GetFileSizeEx(this->hFile, &file_size);
//   if (file_size.QuadPart != requested_size) {
//     ThrowLastError("Incorrect file size");
//   }

//   // Now map the file
//   this->hMap =
//       CreateFileMapping(this->hFile, nullptr, PAGE_READWRITE, 0, 0, nullptr);
//   if (this->hMap == INVALID_HANDLE_VALUE) {
//     ThrowLastError("Failed to map file " + filename);
//   }

//   this->map = (char *)MapViewOfFile(
//       this->hMap, FILE_MAP_ALL_ACCESS, 0, 0, requested_size);
//   if (!this->map) {
//     ThrowLastError("Failed to map view of file " + filename);
//   }

// #else // Platform: POSIX

//   /* Open a file for writing.
//       - Creating the file if it doesn't exist.
//       - Truncating it to 0 size if it already exists. (not really needed)
//      Note: "O_WRONLY" mode is not sufficient when mmaping. */

//   this->fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);

//   if (this->fd == -1) {
//     ThrowLastError("Error opening file for writing");
//   }

//   // Stretch the file size to the size of the (mmapped) array of char
//   if (lseek(this->fd, requested_size - 1, SEEK_SET) == -1) {
//     ThrowLastError("Error calling lseek() to 'stretch' the file");
//   }

//   /* Something needs to be written at the end of the file to
//      have the file actually have the new size.
//      Just writing an empty string at the current file position will do.
//      Note:
//      - The current position in the file is at the end of the stretched
//        file due to the call to lseek().
//      - An empty string is actually a single '\0' character, so a zero-byte
//        will be written at the last byte of the file. */

//   if (write(this->fd, "", 1) == -1) {
//     ThrowLastError("Error writing last byte of the file");
//   }

//   // Now the file is ready to be mmapped.
//   this->map = (char *)mmap(
//       0, requested_size, PROT_READ | PROT_WRITE, MAP_SHARED, this->fd, 0);
//   if (this->map == MAP_FAILED) {
//     ThrowLastError("Error mmapping the file");
//   }

// #endif // defined(_WIN32)

//   this->file_size = requested_size;
// }

// inline void FileRef_VM::read(void *data, const size_t bytes)
// {
//   assert(this->direction == FileRef::READ);
//   assert(bytes <= this->file_size);

//   printf("Read %zu bytes\n", bytes);

//   char *text = (char *)data;
//   for (size_t i = 0; i < bytes; i++) {
//     text[i] = this->map[this->file_p + i];
//   }

//   this->file_p += bytes;
// }

// inline void FileRef_VM::write(const void *data, const size_t bytes)
// {
//   assert(this->direction == FileRef::WRITE);
//   assert(bytes <= this->file_size);

//   printf("Writing %zu bytes\n", bytes);

//   // Write data to in-core memory
//   const char *text = (const char *)data;
//   for (size_t i = 0; i < bytes; i++) {
//     this->map[this->file_p + i] = text[i];
//   }

//   this->file_p += bytes;
// }

// #if defined(_WIN32)
// inline FileReader_ByByte::FileReader_ByByte(const std::string &filename)
// {
//   this->direction = FileRef::READ;

//   this->hFile = CreateFile(filename.c_str(),
//       FILE_ATTRIBUTE_READONLY,
//       0,
//       NULL,
//       OPEN_EXISTING,
//       FILE_FLAG_SEQUENTIAL_SCAN,
//       NULL);

//   if (this->hFile == INVALID_HANDLE_VALUE) {
//     ThrowLastError("failed to open file " + filename);
//   }

//   LARGE_INTEGER file_size;
//   GetFileSizeEx(this->hFile, &file_size);
//   if (file_size.QuadPart == 0) {
//     ThrowLastError("cannot map 0 size file");
//   }
//   this->file_size = file_size.QuadPart;
// }

// inline void FileReader_ByByte::read(void *data, const size_t bytes)
// {
//   assert(this->direction == FileRef::READ);
//   assert(bytes <= this->file_size);

//   printf("read %zu bytes\n", bytes);

//   char *text = (char *)data;

//   size_t p = 0;

//   while (p < bytes) {
//     // compute how many bypes to read in the next stream
//     const size_t avail = std::min(
//         this->file_size - this->file_p, size_t(this->STREAM_BUFFER_SIZE));

//     // of course the stream progress should be resetted
//     if (this->stream_p >= avail) {
//       this->stream_p = 0;
//     }

//     // how many bytes to copy from the buffer
//     const size_t bytes_to_read =
//         std::min(bytes - p, size_t(avail - this->stream_p));

//     // whether we can skup the copy in this round
//     bool skip_copy = false;

//     if (this->stream_p == 0) {
//       // when the data needed equals to the data to be streamed, we can
//       // directly use the destination as the stream buffer. therefore here
//       // we use a pointer to indicate the buffer to use for streaming.
//       char *buffer = this->stream_buffer;
//       if (bytes_to_read == this->STREAM_BUFFER_SIZE) {
//         buffer = &text[p];
//         skip_copy = true;
//       }

//       // actually stream data from file to the `buffer`
//       DWORD bytes_read;
//       if (!ReadFile(this->hFile,
//               buffer,
//               this->STREAM_BUFFER_SIZE,
//               &bytes_read,
//               NULL)) {
//         ThrowLastError("failed to load streaming buffer");
//       }
//       assert(bytes_read == avail);

//       // always update the file progress
//       this->file_p += bytes_read;
//     }

//     // copy data from the stream buffer to the destination because
//     // additional bytes are streamed.
//     if (!skip_copy) {
//       for (size_t i = 0; i < bytes_to_read; i++) {
//         text[p + i] = this->stream_buffer[this->stream_p + i];
//       }
//     }

//     this->stream_p += bytes_to_read;
//     p += bytes_to_read;
//   }
// }

// inline void FileReader_ByByte::write(const void *data, size_t bytes)
// {
//   throw std::runtime_error("write is not supported");
// }
// #endif // defined(_WIN32)

// inline FileMap filemap_write_create(
//     const std::string &filename, size_t requested_size)
// {
//   FileMap ret = new FileRef_VM(filename, requested_size);
//   return ret;
// }

// inline FileMap filemap_read_create(const std::string &filename)
// {
//   // FileMap ret = new FileRef_VM(filename);
//   FileMap ret = new FileReader_ByByte(filename);
//   return ret;
// }

// inline void filemap_write(FileMap &file, const void *data, const size_t bytes)
// {
//   file->write(data, bytes);
// }

// inline void filemap_read(FileMap &file, void *data, const size_t bytes)
// {
//   file->read(data, bytes);
// }

// inline void filemap_close(FileMap &file)
// {
//   delete file;
// }

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
  vec3f gridScaling = (float)domainRes;

  cpp::Volume volume("htg");
  volume.setParam("filter", (int)OSP_VOLUME_FILTER_TRILINEAR);
  volume.setParam("gradientFilter", (int)OSP_VOLUME_FILTER_TRILINEAR);
  volume.setParam("gridOrigin", vec3f(0, 0, 0));
  volume.setParam("gridScaling", gridScaling);
  volume.setParam("nodeFile", "C:/Datasets/bunny_cloud.htg");
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