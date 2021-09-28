#ifndef VIDI_MEMORY_STAT_H
#define VIDI_MEMORY_STAT_H

#if __linux__ || __sun
#include <sys/resource.h>
#include <unistd.h>
#include <sys/utsname.h> /* for uname */
#include <errno.h> /* for use in LinuxKernelVersion() */

#elif __APPLE__ && !__ARM_ARCH
#include <unistd.h>
#include <mach/mach.h>
#include <AvailabilityMacros.h>
#if MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_6 || __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_8_0
#include <mach/shared_region.h>
#else
#include <mach/shared_memory_server.h>
#endif
#if SHARED_TEXT_REGION_SIZE || SHARED_DATA_REGION_SIZE
const size_t shared_size = SHARED_TEXT_REGION_SIZE + SHARED_DATA_REGION_SIZE;
#else
const size_t shared_size = 0;
#endif

#elif _WIN32
#include <windows.h>
#include <psapi.h>
#if _MSC_VER
#pragma comment(lib, "psapi")
#endif

#endif /* OS selection */

// #include <cassert>

namespace vidi {
namespace details {

inline void ReportError(const char *filename, int line, const char *expression, const char *message)
{
#ifndef NDEBUG
  fprintf(stderr,
          "%s:%d, assertion %s: %s\n",
          filename,
          line,
          expression,
          message ? message : "failed");
  std::runtime_error("failed");
#else
  ;
#endif
}

static const char *c_str(const std::string &s)
{
  return s.c_str();
}

template <typename T>
static T c_str(T s)
{
  return s;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"

template <typename... Ts>
static std::string stringf(const std::string &format, Ts... rest)
{
  ssize_t sz = snprintf(NULL, 0, format.c_str(), c_str(rest)...);
  char *bf = static_cast<char *>(malloc(sz + 1));
  snprintf(bf, sz + 1, format.c_str(), c_str(rest)...);
  std::string ret(bf);
  free(bf);
  return ret;
}

#pragma GCC diagnostic pop
  
}
} // namespace vidi

#define ASSERT_CUSTOM(p, message, file, line) \
  ((p) ? (void)0 : vidi::details::ReportError(file, line, #p, message))

#define ASSERT(p, message) ASSERT_CUSTOM(p, message, __FILE__, __LINE__)

namespace vidi {

enum MemoryStatType
{
  currentUsage,
  peakUsage
};

#if __linux__
inline unsigned LinuxKernelVersion()
{
  unsigned digit1, digit2, digit3;
  struct utsname utsnameBuf;
  if (-1 == uname(&utsnameBuf)) {
    ASSERT(false, vidi::details::stringf("%s%s", "Can't call uname: errno = ", errno).c_str());
  }
  if (3 != sscanf(utsnameBuf.release, "%u.%u.%u", &digit1, &digit2, &digit3)) {
    ASSERT(false, vidi::details::stringf("%s%s", "Unable to parse OS release: ", utsnameBuf.release).c_str());
  }
  return 1000000 * digit1 + 1000 * digit2 + digit3;
}
#endif

/* Return estimate of number of bytes of memory that this program is currently using.
 * Returns 0 if not implemented on platform. */
inline size_t GetMemoryUsage(MemoryStatType stat = currentUsage)
{
  ASSERT(stat == currentUsage || stat == peakUsage, NULL);
#if _WIN32
  PROCESS_MEMORY_COUNTERS mem;
  bool status = GetProcessMemoryInfo(GetCurrentProcess(), &mem, sizeof(mem)) != 0;
  ASSERT(status, NULL);
  return stat == currentUsage ? mem.PagefileUsage : mem.PeakPagefileUsage;
#elif __linux__
  long unsigned size = 0;
  FILE *fst = fopen("/proc/self/status", "r");
  ASSERT(fst != nullptr, NULL);
  const int BUF_SZ = 200;
  char buf_stat[BUF_SZ];
  const char *pattern = stat == peakUsage ? "VmPeak: %lu" : "VmSize: %lu";
  while (NULL != fgets(buf_stat, BUF_SZ, fst)) {
    if (1 == sscanf(buf_stat, pattern, &size)) {
      ASSERT(size, "Invalid value of memory consumption.");
      break;
    }
  }
  // VmPeak is available in kernels staring 2.6.15
  if (stat != peakUsage || LinuxKernelVersion() >= 2006015)
    ASSERT(size, "Invalid /proc/self/status format, pattern not found.");
  fclose(fst);
  return size * 1024;
#elif __APPLE__ && !__ARM_ARCH
  // TODO: find how detect peak virtual memory size under macOS
  if (stat == peakUsage)
    return 0;
  kern_return_t status;
  task_basic_info info;
  mach_msg_type_number_t msg_type = TASK_BASIC_INFO_COUNT;
  status =
      task_info(mach_task_self(), TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &msg_type);
  ASSERT(status == KERN_SUCCESS, NULL);
  return info.virtual_size - shared_size;
#else
  fprintf(stderr, "memory profile not supported on this platform\n");
  return 0;
#endif
}

enum MemorySizeFormat {
  BYTES = 0,
  KB,
  MB,
  GB,
};

template<MemorySizeFormat format>
float GetPrettyMemoryUsage(MemoryStatType stat = currentUsage);

template<>
inline float GetPrettyMemoryUsage<MemorySizeFormat::BYTES>(MemoryStatType stat)
{
  return GetMemoryUsage(stat);
}

template<>
inline float GetPrettyMemoryUsage<MemorySizeFormat::KB>(MemoryStatType stat)
{
  return GetMemoryUsage(stat) / 1e3;
}

template<>
inline float GetPrettyMemoryUsage<MemorySizeFormat::MB>(MemoryStatType stat)
{
  return GetMemoryUsage(stat) / 1e6;
}

template<>
inline float GetPrettyMemoryUsage<MemorySizeFormat::GB>(MemoryStatType stat)
{
  return GetMemoryUsage(stat) / 1e9;
}

/** Use approximately a specified amount of stack space.
 *  Recursion is used here instead of alloca because some
 *  implementations of alloca do not use the stack. */
inline void UseStackSpace(size_t amount, char *top = 0)
{
  char x[1000];
  memset(x, -1, sizeof(x));
  if (!top)
    top = x;
  ASSERT(x <= top, "test assumes that stacks grow downwards");
  if (size_t(top - x) < amount)
    UseStackSpace(amount, top);
}

}  // namespace vidi

#endif  // VIDI_MEMORY_STAT_H
