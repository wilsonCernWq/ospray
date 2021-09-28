#ifndef VIDI_FILEMAP_H
#define VIDI_FILEMAP_H

#include <string>
#include <memory>
#include <atomic>

#if !defined(_WIN32)
#include <sys/mman.h>
#endif

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE -1
#endif

namespace vidi {

#if defined(_WIN32)  // Platform: Windows
typedef HANDLE FileDesc;
#else  // Platform: POSIX
typedef int FileDesc;
#endif  // defined(_WIN32)

struct FileRef;
typedef std::shared_ptr<FileRef> FileMap;

// ----------------------------------------------------------------------------
// File I/O
// ----------------------------------------------------------------------------
void ThrowLastError(const std::string &msg);

struct FutureBuffer
{
 protected:
  const char *ptr{nullptr};
  size_t size;

 public:
  FutureBuffer(size_t length, const void *target) : ptr((const char *)target), size(length) {}

  const char *const get() const
  {
    return ptr;
  }

  size_t numOfBytes() const
  {
    return size;
  }

  virtual bool ready() const
  {
    return true;
  }

  virtual void wait() const
  {
    ;
  }

  virtual void cancel(FileDesc) const
  {
    ;
  }

  virtual void cancel(FileMap) const
  {
    ;
  }
};

typedef std::shared_ptr<FutureBuffer> future_buffer_t;

template <typename T>
future_buffer_t makeBasicFutureBuffer(T &&x)
{
  return std::make_shared<FutureBuffer>(std::forward<T>(x));
}

template <typename... Args>
future_buffer_t makeBasicFutureBuffer(Args &&... args)
{
  return std::make_shared<FutureBuffer>(std::forward<Args>(args)...);
}

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

struct FileRef
{
  FileDesc fd = INVALID_HANDLE_VALUE;
  enum Direction
  {
    READ,
    WRITE,
    // WRITE_APPEND
  };

 protected:
  const size_t file_size = 0;
  const static bool verbose = false;
  const Direction direction;

 public:
  FileRef(const std::string &filename, FileDesc h, Direction mode);

  FileRef(const std::string &filename, FileDesc h, size_t size, Direction mode);

  virtual ~FileRef();

  virtual void setFilePointer(size_t offset) = 0;

  virtual size_t getFilePointer() = 0;

  virtual future_buffer_t readData(void *data, size_t bytes, size_t _unused) = 0;

  virtual future_buffer_t writeData(const void *data, size_t bytes, size_t _unused) = 0;

  virtual future_buffer_t randomRead(size_t offset, size_t bytes, void *data)
  {
    setFilePointer(offset);
    return readData(data, bytes, size_t(-1));
  }

  virtual future_buffer_t randomWrite(size_t offset, size_t bytes, const void *data)
  {
    setFilePointer(offset);
    return writeData(data, bytes, size_t(-1));
  }
};

// ----------------------------------------------------------------------------
//
// File I/O using virtual memories (mmap)
//
// ----------------------------------------------------------------------------

struct FileRef_VM : public FileRef
{
#if defined(_WIN32)
  HANDLE hMap = INVALID_HANDLE_VALUE;
  char *map = NULL;
#else  // Platform: Unix
  char *map = (char *)MAP_FAILED;
#endif  // defined(_WIN32)

  /* should not keep a copy of the file pointer */
  size_t file_p = 0;

 public:
  ~FileRef_VM();

  /* reader */
  FileRef_VM(const std::string &filename);

  /* writer */
  FileRef_VM(const std::string &filename, size_t requested_size);

  future_buffer_t readData(void *data, size_t bytes, size_t _unused) override;

  future_buffer_t writeData(const void *data, size_t bytes, size_t _unused) override;

  future_buffer_t randomRead(size_t offset, size_t bytes, void *data) override;

  future_buffer_t randomWrite(size_t offset, size_t bytes, const void *data) override;

  void setFilePointer(size_t offset) override;

  size_t getFilePointer() override;
};

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

struct FileRef_ByByte : public FileRef
{
#if defined(_WIN32)
  static const DWORD STREAM_BUFFER_SIZE = 512 * 1024;
  char stream_buffer[STREAM_BUFFER_SIZE];
  DWORD stream_p = 0;
#else  // Platform: Unix
  static const size_t STREAM_BUFFER_SIZE = 512 * 1024;
  char stream_buffer[STREAM_BUFFER_SIZE];
  size_t stream_p = 0;
#endif  // defined(_WIN32)
  int64_t stream_rank = int64_t(-1);
  size_t file_p = 0;

 public:
  FileRef_ByByte(const std::string &filename);

  future_buffer_t readData(void *data, size_t bytes, size_t _unused) override;

  future_buffer_t writeData(const void *data, size_t bytes, size_t _unused) override;

  void setFilePointer(size_t offset) override;

  size_t getFilePointer() override;

 private:
  void Stream(char *buffer);
};

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

#if defined(_WIN32)
struct FileRef_Async : public FileRef
{
  struct AsyncFutureBuffer : public FutureBuffer
  {
    HANDLE hFile;
    std::shared_ptr<OVERLAPPED> overlapped_structure;

   private:
    void create(size_t _offset)
    {
      LARGE_INTEGER offset;
      offset.QuadPart = _offset;

      this->overlapped_structure = std::make_shared<OVERLAPPED>();
      memset(this->overlapped_structure.get(), 0, sizeof(OVERLAPPED));
      this->overlapped_structure->Offset = offset.LowPart;
      this->overlapped_structure->OffsetHigh = offset.HighPart;
      this->overlapped_structure->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    }

   public:
    AsyncFutureBuffer(HANDLE hFile, size_t offset, size_t bytes, const void *target)
        : FutureBuffer(bytes, target), hFile(hFile)
    {
      create(offset);
    }

    bool ready() const override
    {
      return HasOverlappedIoCompleted(overlapped_structure.get());
    }

    void wait() const override
    {
      DWORD numOfBytesRead;
      DWORD err = GetOverlappedResult(hFile, overlapped_structure.get(), &numOfBytesRead, true);
      if (!err) {
        ThrowLastError("async IO failed");
      }
      assert(numOfBytesRead == size);
    }

    void cancel(FileDesc fd) const override
    {
      BOOL result = CancelIoEx(fd, overlapped_structure.get());
      if (result == TRUE || GetLastError() != ERROR_NOT_FOUND) {
        return;
      }
      ThrowLastError("failed to cancel IO");
      // Wait for the I/O subsystem to acknowledge our cancellation.
      // Depending on the timing of the calls, the I/O might complete with a
      // cancellation status, or it might complete normally (if the ReadFile was
      // in the process of completing at the time CancelIoEx was called, or if
      // the device does not support cancellation).
      // This call specifies TRUE for the bWait parameter, which will block
      // until the I/O either completes or is canceled, thus resuming execution,
      // provided the underlying device driver and associated hardware are functioning
      // properly. If there is a problem with the driver it is better to stop
      // responding here than to try to continue while masking the problem.
    }

    void cancel(FileMap hfile) const override
    {
      cancel(hfile->fd);
    }
  };

 public:
  FileRef_Async(const std::string &filename);

  FileRef_Async(const std::string &filename, size_t requested_size);

  void setFilePointer(size_t offset) override;

  size_t getFilePointer() override;

  future_buffer_t readData(void *data, size_t bytes, size_t offset) override;

  future_buffer_t writeData(const void *data, size_t bytes, size_t offset) override;

  future_buffer_t randomRead(size_t offset, size_t bytes, void *data) override
  {
    return readData(data, bytes, offset);
  }

  future_buffer_t randomWrite(size_t offset, size_t bytes, const void *data) override
  {
    return writeData(data, bytes, offset);
  }
};
#endif

// ----------------------------------------------------------------------------
//
// API
//
// ----------------------------------------------------------------------------

inline FileMap filemap_write_create(const std::string &filename, size_t requested_size)
{
  // return std::make_shared<FileRef_Async>(filename, requested_size);
  return std::make_shared<FileRef_VM>(filename, requested_size);
}

inline FileMap filemap_read_create(const std::string &filename)
{
  // return std::make_shared<FileRef_Async>(filename);
  return std::make_shared<FileRef_VM>(filename);
}

inline FileMap filemap_write_create_async(const std::string &filename, size_t requested_size)
{
#if defined(_WIN32)  // Platform: Windows
  // return std::make_shared<FileRef_VM>(filename, requested_size);
  return std::make_shared<FileRef_Async>(filename, requested_size);
#else
  return std::make_shared<FileRef_VM>(filename, requested_size);
#endif
}

inline FileMap filemap_read_create_async(const std::string &filename)
{
#if defined(_WIN32)  // Platform: Windows
  // return std::make_shared<FileRef_VM>(filename);
  return std::make_shared<FileRef_Async>(filename);
#else
  return std::make_shared<FileRef_VM>(filename);
#endif
}

inline void filemap_close(FileMap &file)
{
  file.reset();
}

/* deprecated */
inline void filemap_write(FileMap &file, const void *data, const size_t bytes)
{
  auto r = file->writeData(data, bytes, size_t(-1));
  r->wait();
}

inline void filemap_read(FileMap &file, void *data, const size_t bytes)
{
  auto r = file->readData(data, bytes, size_t(-1));
  r->wait();
}

/* synchronous */
inline void filemap_random_read(FileMap &file, size_t offset, void *data, const size_t bytes)
{
  // auto r = file->randomRead(offset, bytes, data);
  // r->wait();
  const size_t batch = 1830620256;
  for (size_t o = 0; o < bytes; o += batch) {
    auto r = file->randomRead(offset + o, std::min(batch, bytes - o), ((uint8_t *)data) + o);
    r->wait();
  }
}

inline void filemap_random_write(FileMap &file, size_t offset, const void *data, const size_t bytes)
{
  // auto r = file->randomWrite(offset, bytes, data);
  // r->wait();
  const size_t batch = 1830620256;
  for (size_t o = 0; o < bytes; o += batch) {
    auto r = file->randomWrite(offset + o, std::min(batch, bytes - o), ((uint8_t *)data) + o);
    r->wait();
  }
}

inline void filemap_random_write_update(FileMap &file,
                                        size_t &offset,
                                        const void *data,
                                        const size_t bytes)
{
  filemap_random_write(file, offset, data, bytes);
  offset += bytes;
}

inline void filemap_random_read_update(FileMap &file,
                                       size_t &offset,
                                       void *data,
                                       const size_t bytes)
{
  filemap_random_read(file, offset, data, bytes);
  offset += bytes;
}

/* asynchronous */
inline future_buffer_t filemap_random_read_async(FileMap &file,
                                                 size_t offset,
                                                 void *data,
                                                 const size_t bytes)
{
  return file->randomRead(offset, bytes, data);
}

inline future_buffer_t filemap_random_write_async(FileMap &file,
                                                  size_t offset,
                                                  void *data,
                                                  const size_t bytes)
{
  return file->randomWrite(offset, bytes, data);
}

}  // namespace vidi

#endif  // VIDI_FILEMAP_H

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#ifdef VIDI_FILEMAP_IMPLEMENTATION

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

// #include <stdio.h>  // For printf()
// #include <string.h>  // For memcmp()
// #include <stdlib.h>  // For exit()

namespace vidi {

void ThrowLastError(const std::string &msg)
{
#if defined(_WIN32)  // Create a string with last error message
  auto result = std::string("no error");
  DWORD error = GetLastError();
  if (error) {
    LPVOID lpMsgBuf;
    DWORD bufLen = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0,
        NULL);
    if (bufLen) {
      LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
      result = lpMsgStr, lpMsgStr + bufLen;
      LocalFree(lpMsgBuf);
    }
  }
  printf("%s: %s\n", msg.c_str(), result.c_str());
  throw std::runtime_error(msg + ": " + result);
#else
  perror(msg.c_str());
  throw std::runtime_error("Termination caused by I/O errors");
#endif  // defined(_WIN32)
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------
static size_t read_file_size(const std::string &filename, FileDesc h)
{
  if (h == INVALID_HANDLE_VALUE) {
    ThrowLastError("failed to map file " + filename);
  }
#if defined(_WIN32)  // Platform: Windows
  LARGE_INTEGER size;
  GetFileSizeEx(h, &size);
  return size.QuadPart;
#else  // Platform: POSIX
  struct stat size = {0};
  if (fstat(h, &size) == -1) {
    ThrowLastError("error getting the file size");
  }
  return size.st_size;
#endif  // defined(_WIN32)
}

static void stretch_file_size(FileDesc h, size_t requested_size)
{
#if defined(_WIN32)  // Platform: Windows
  // Stretch the file size to the size of the (mapped) array of char
  LARGE_INTEGER file_size;
  file_size.QuadPart = requested_size;
  if (!SetFilePointerEx(h, file_size, NULL, FILE_BEGIN)) {
    ThrowLastError("error calling SetFilePointerEx() to 'stretch' the file");
  }

  // Actually stretch the file
  SetEndOfFile(h);

  // Verify file size
  GetFileSizeEx(h, &file_size);
  if (file_size.QuadPart != requested_size) {
    ThrowLastError("incorrect file size");
  }

#else  // Platform: POSIX

  // Stretch the file size to the size of the (mmapped) array of char
  if (::lseek(h, requested_size - 1, SEEK_SET) == -1) {
    ThrowLastError("error calling lseek() to 'stretch' the file");
  }

  /* Something needs to be written at the end of the file to
     have the file actually have the new size.
     Just writing an empty string at the current file position will do.
     Note:
     - The current position in the file is at the end of the stretched
       file due to the call to lseek().
     - An empty string is actually a single '\0' character, so a zero-byte
       will be written at the last byte of the file. */

  if (::write(h, "", 1) == -1) {
    ThrowLastError("error writing last byte of the file");
  }

#endif  // Platform: Windows
}

FileRef::FileRef(const std::string &filename, FileDesc h, Direction mode)
    : fd(h), file_size(read_file_size(filename, h)), direction(mode)
{
  assert(mode == Direction::READ);
  if (verbose)
    printf("file size is %zu\n", file_size);
}

FileRef::FileRef(const std::string &filename, FileDesc h, size_t size, Direction mode)
    : fd(h), file_size(size), direction(mode)
{
  assert(mode == Direction::WRITE);
}

FileRef::~FileRef()
{
#if defined(_WIN32)  // Platform: Windows
  FlushFileBuffers(this->fd);
  CloseHandle(this->fd);
#else  // Platform: POSIX
  // Un-mmaping doesn't close the file, so we still need to do that.
  close(this->fd);
#endif  // defined(_WIN32)
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

FileRef_VM::~FileRef_VM()
{
#if defined(_WIN32)  // Platform: Windows
  UnmapViewOfFile(this->map);
  CloseHandle(this->hMap);
#else  // Platform: POSIX
  // Don't forget to free the mmapped memory
  if (munmap(this->map, this->file_size) == -1) {
    ThrowLastError("error un-mmapping the file");
  }
#endif  // defined(_WIN32)
}

/* reader */
FileRef_VM::FileRef_VM(const std::string &filename)
    : FileRef(filename,
#if defined(_WIN32)  // Platform: Windows
              CreateFile(filename.c_str(),
                         FILE_ATTRIBUTE_READONLY,
                         FILE_SHARE_READ,
                         NULL,
                         OPEN_EXISTING,
                         FILE_FLAG_SEQUENTIAL_SCAN,
                         NULL),
#else
              open(filename.c_str(), O_RDONLY, (mode_t)0600),
#endif
              FileRef::READ)
{
  if (this->file_size == 0) {
    throw std::runtime_error("cannot map 0 size file");
  }

  // Now map the file to virtual memory
#if defined(_WIN32)  // Platform: Windows
  this->hMap = CreateFileMapping(this->fd, nullptr, PAGE_READONLY, 0, 0, nullptr);
  if (this->hMap == INVALID_HANDLE_VALUE) {
    ThrowLastError("failed to map file " + filename);
  }
  this->map = (char *)MapViewOfFile(this->hMap, FILE_MAP_READ, 0, 0, this->file_size);
  if (!this->map) {
    ThrowLastError("failed to map file " + filename);
  }
#else  // Platform: POSIX
  this->map = (char *)mmap(0, this->file_size, PROT_READ, MAP_SHARED, fd, 0);
  if (this->map == MAP_FAILED) {
    ThrowLastError("failed to map of file " + filename);
  }
#endif  // defined(_WIN32)
}

/* writer */
FileRef_VM::FileRef_VM(const std::string &filename, size_t requested_size)
    : FileRef(filename,
#if defined(_WIN32)  // Platform: Windows
              CreateFile(filename.c_str(),
                         GENERIC_WRITE | GENERIC_READ,
                         FILE_SHARE_WRITE,
                         NULL,
                         CREATE_ALWAYS,
                         FILE_FLAG_WRITE_THROUGH,
                         NULL),
#else
              /* Open a file for writing. Note: "O_WRONLY" mode is not sufficient when mmaping.
               - Creating the file if it doesn't exist.
               - Truncating it to 0 size if it already exists. (not really needed) */
              open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600),
#endif
              requested_size,
              FileRef::WRITE)
{
  if (this->fd == INVALID_HANDLE_VALUE) {
    ThrowLastError("failed to open file " + filename);
  }
  stretch_file_size(this->fd, requested_size);

#if defined(_WIN32)  // Platform: Windows
  // Now map the file
  this->hMap = CreateFileMapping(this->fd, nullptr, PAGE_READWRITE, 0, 0, nullptr);
  if (this->hMap == INVALID_HANDLE_VALUE) {
    ThrowLastError("failed to map file " + filename);
  }
  this->map = (char *)MapViewOfFile(this->hMap, FILE_MAP_ALL_ACCESS, 0, 0, requested_size);
  if (!this->map) {
    ThrowLastError("failed to map of file " + filename);
  }
#else  // Platform: POSIX
  // Now the file is ready to be mmapped.
  this->map = (char *)mmap(0, requested_size, PROT_READ | PROT_WRITE, MAP_SHARED, this->fd, 0);
  if (this->map == MAP_FAILED) {
    ThrowLastError("failed to map of file " + filename);
  }
#endif  // defined(_WIN32)
}

future_buffer_t FileRef_VM::readData(void *data, size_t bytes, size_t _unused)
{
  assert(this->direction == FileRef::READ);
  assert(bytes <= this->file_size);
  assert(_unused == size_t(-1));
  if (verbose) {
    printf("read %zu bytes\n", bytes);
  }
  char *text = (char *)data;
  for (size_t i = 0; i < bytes; i++) {
    text[i] = this->map[this->file_p + i];
  }
  this->file_p += bytes;
  return makeBasicFutureBuffer(bytes, data);

  // throw std::runtime_error("FileRef_VM does not support sequential read");
}

future_buffer_t FileRef_VM::writeData(const void *data, size_t bytes, size_t _unused)
{
  assert(this->direction == FileRef::WRITE);
  assert(bytes <= this->file_size);
  assert(_unused == size_t(-1));
  if (verbose) {
    printf("writing %zu bytes\n", bytes);
  }

  // Write data to in-core memory
  const char *text = (const char *)data;
  for (size_t i = 0; i < bytes; i++) {
    this->map[this->file_p + i] = text[i];
  }
  this->file_p += bytes;
  return makeBasicFutureBuffer(bytes, data);

  // throw std::runtime_error("FileRef_VM does not support sequential write");
}

void FileRef_VM::setFilePointer(size_t offset)
{
  assert(offset < this->file_size);
  this->file_p = offset;
  // throw std::runtime_error("FileRef_VM does not support file pointer");
}

size_t FileRef_VM::getFilePointer()
{
  return this->file_p;
  // throw std::runtime_error("FileRef_VM does not support file pointer");
}

future_buffer_t FileRef_VM::randomRead(size_t offset, size_t bytes, void *data)
{
  assert(this->direction == FileRef::READ);
  assert(bytes <= this->file_size);

  if (verbose) {
    printf("read %zu bytes\n", bytes);
  }

  std::memcpy((char *)data, this->map + offset, bytes);

  return makeBasicFutureBuffer(bytes, data);
}

future_buffer_t FileRef_VM::randomWrite(size_t offset, size_t bytes, const void *data)
{
  assert(this->direction == FileRef::WRITE);
  assert(bytes <= this->file_size);

  if (verbose) {
    printf("writing %zu bytes\n", bytes);
  }

  std::memcpy(this->map + offset, (char *)data, bytes);

  return makeBasicFutureBuffer(bytes, data);
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

FileRef_ByByte::FileRef_ByByte(const std::string &filename)
    : FileRef(filename,
#if defined(_WIN32)
              CreateFile(filename.c_str(),
                         FILE_ATTRIBUTE_READONLY,
                         FILE_SHARE_READ,
                         NULL,
                         OPEN_EXISTING,
                         /* The file or device attributes and flags */
                         FILE_ATTRIBUTE_NORMAL,
                         NULL),
#else
              open(filename.c_str(), O_RDONLY, (mode_t)0600),
#endif
              FileRef::READ)
{
  if (this->fd == INVALID_HANDLE_VALUE) {
    ThrowLastError("failed to open file " + filename);
  }

  if (this->file_size == 0) {
    ThrowLastError("error: file is empty, nothing to do");
  }
}

future_buffer_t FileRef_ByByte::readData(void *data, size_t bytes, size_t _unused)
{
  assert(this->direction == FileRef::READ);
  assert(bytes <= this->file_size);
  assert(_unused == size_t(-1));

  if (verbose)
    printf("read %zu bytes\n", bytes);

  char *text = (char *)data;

  size_t p = 0;

  int rank = (this->stream_rank == -1) ? 0 : this->stream_rank;

  while (p < bytes) {
    // compute how many bypes to read in the next stream
    const size_t avail_till_eof = (rank != this->stream_rank)
        ? this->file_size - this->file_p
        : this->file_size - this->file_p + this->STREAM_BUFFER_SIZE;
    const size_t avail = std::min(avail_till_eof, size_t(this->STREAM_BUFFER_SIZE));

    // of course the stream progress should be resetted
    if (this->stream_p >= avail) {
      this->stream_p = 0;
      ++rank;
    }

    // how many bytes to copy from the buffer
    const size_t bytes_to_read = std::min(bytes - p, size_t(avail - this->stream_p));

    // whether we can skup the copy in this round
    bool skip_copy = false;

    if (rank != this->stream_rank) {
      // when the data needed equals to the data to be streamed, we can
      // directly use the destination as the stream buffer. therefore here
      // we use a pointer to indicate the buffer to use for streaming.
      char *buffer = this->stream_buffer;
      if (bytes_to_read == this->STREAM_BUFFER_SIZE) {
        buffer = &text[p];
        skip_copy = true;
      }

      // actually stream data from file to the `buffer`
      this->Stream(buffer);
    }

    // copy data from the stream buffer to the destination because
    // additional bytes are streamed.
    if (!skip_copy) {
      for (size_t i = 0; i < bytes_to_read; i++) {
        text[p + i] = this->stream_buffer[this->stream_p + i];
      }
    }

    this->stream_p += bytes_to_read;
    p += bytes_to_read;
  }

  return makeBasicFutureBuffer(bytes, data);
}

future_buffer_t FileRef_ByByte::writeData(const void *data, size_t bytes, size_t _unused)
{
  throw std::runtime_error("write is not supported");
}

void FileRef_ByByte::setFilePointer(size_t offset)
{
  assert(offset < this->file_size);
  // std::cerr << "FileRef_ByByte::setFilePointer is untested" << std::endl;

  // the correct stream cursor position
  this->stream_p = offset % this->STREAM_BUFFER_SIZE;

  // find the actual offset
  //   if the stream buffer has not been loaded ==> last possible file pointer
  //   position otherwise ==> next possible file pointer position
  const int64_t _rank = offset / this->STREAM_BUFFER_SIZE;
  const size_t _offset =
      (_rank != this->stream_rank ? _rank : _rank + 1) * this->STREAM_BUFFER_SIZE;

  ////
  //// set file pointer position correctly ////
  ////

#if defined(_WIN32)

  if (this->fd == INVALID_HANDLE_VALUE) {
    ThrowLastError("invalid file handler");
  }

  LARGE_INTEGER file_offset;
  file_offset.QuadPart = _offset;
  if (!SetFilePointerEx(this->fd, file_offset, NULL, FILE_BEGIN)) {
    ThrowLastError("error resetting the file pointer");
  }

#else  // Platform: Unix

  if (this->fd == -1) {
    ThrowLastError("invalid file handler");
  }

  off_t ret = ::lseek(this->fd, _offset, SEEK_SET);
  if (ret != (off_t)_offset) {
    ThrowLastError("error resetting the file pointer");
  }

#endif  // defined(_WIN32)

  this->file_p = _offset;

  ////
  //// check file pointer position & the stream buffer ////
  ////

  if (_rank != this->stream_rank) {
    /* we need to reload stream buffer */
    Stream(this->stream_buffer);
  }

  assert(this->getFilePointer() == this->file_size ||
         this->getFilePointer() % this->STREAM_BUFFER_SIZE == 0);
}

size_t FileRef_ByByte::getFilePointer()
{
#if !defined(NDEBUG) || defined(_DEBUG)
  /* safe version */
  size_t p;
#if defined(_WIN32)
  if (this->fd == INVALID_HANDLE_VALUE) {
    ThrowLastError("invalid file handler");
  }
  if (this->file_p == this->file_size) {
    p = this->file_size;
  } else {
    LARGE_INTEGER zero, file_pointer;
    zero.QuadPart = 0;
    if (!SetFilePointerEx(this->fd, zero, &file_pointer, FILE_CURRENT)) {
      ThrowLastError("error retrieving the file pointer");
    }
    p = file_pointer.QuadPart;
  }
#else  // Platform: Unix
  if (this->fd == -1) {
    ThrowLastError("invalid file handler");
  }
  p = ::lseek(this->fd, 0, SEEK_CUR);
#endif  // defined(_WIN32)
  assert(p == this->file_p);
  return p;
#else
  /* fast version */
  return this->file_p;
#endif
}

void FileRef_ByByte::Stream(char *buffer)
{
  const size_t avail = std::min(this->file_size - this->file_p, size_t(this->STREAM_BUFFER_SIZE));

#if defined(_WIN32)

  DWORD br;
  if (!ReadFile(this->fd, buffer, this->STREAM_BUFFER_SIZE, &br, NULL)) {
    ThrowLastError("failed to load streaming buffer");
  }
  assert(br == avail);

#else  // Platform: Unix

  const ssize_t br = read(this->fd, buffer, this->STREAM_BUFFER_SIZE);
  if (br == -1) {
    ThrowLastError("failed to load streaming buffer");
  }
  assert(br == avail);

#endif  // defined(_WIN32)

  this->stream_rank = this->file_p / this->STREAM_BUFFER_SIZE;
  this->file_p += br;
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------
#if defined(_WIN32)
FileRef_Async::FileRef_Async(const std::string &filename)
    : FileRef(filename,
              CreateFile(filename.c_str(),
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL),
              Direction::READ)
{
  if (this->fd == INVALID_HANDLE_VALUE) {
    ThrowLastError("failed to open file " + filename);
  }

  if (this->file_size == 0) {
    ThrowLastError("error: file is empty, nothing to do");
  }
}

FileRef_Async::FileRef_Async(const std::string &filename, size_t requested_size)
    : FileRef(filename,
              CreateFile(filename.c_str(),
                         GENERIC_WRITE | GENERIC_READ,
                         FILE_SHARE_WRITE,
                         NULL,
                         CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL),
              requested_size,
              Direction::WRITE)
{
}

future_buffer_t FileRef_Async::readData(void *data, size_t bytes, size_t offset)
{
  assert(this->direction == FileRef::READ);
  assert(bytes <= this->file_size);
  assert(offset != size_t(-1));

  auto ret = std::make_shared<AsyncFutureBuffer>(this->fd, offset, bytes, data);

  DWORD numBytesRead;
  DWORD err = ReadFile(this->fd, data, bytes, &numBytesRead, ret->overlapped_structure.get());
  if (err == FALSE && GetLastError() != ERROR_IO_PENDING) {
    /* something is wrong */
    ThrowLastError("failed to start async read");
  }

  return ret;
}

future_buffer_t FileRef_Async::writeData(const void *data, size_t bytes, size_t offset)
{
  assert(this->direction == FileRef::WRITE);
  assert(bytes <= this->file_size);
  assert(offset != size_t(-1));

  auto ret = std::make_shared<AsyncFutureBuffer>(this->fd, offset, bytes, data);

  DWORD err = WriteFile(this->fd, data, bytes, NULL, ret->overlapped_structure.get());
  if (err == FALSE && GetLastError() != ERROR_IO_PENDING) {
    /* something is wrong */
    ThrowLastError("failed to start async write");
  }

  return ret;
}

void FileRef_Async::setFilePointer(size_t offset)
{
  throw std::runtime_error("accessing file pointer on async filemap is inappropriate");
}

size_t FileRef_Async::getFilePointer()
{
  throw std::runtime_error("accessing file pointer on async filemap is inappropriate");
}
#endif

}  // namespace vidi

#endif  // VIDI_FILEMAP_IMPLEMENTATION
