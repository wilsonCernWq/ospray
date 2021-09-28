#ifndef VIDI_HIGH_PERFORMANCE_TIMER_H
#define VIDI_HIGH_PERFORMANCE_TIMER_H

#include <chrono>

namespace vidi {

struct HighPerformanceTimer
{
 private:
#if defined(_WIN32)
  LARGE_INTEGER beginClock, endClock, cpuClockFreq, wallTime_ticks;
#else
  std::chrono::high_resolution_clock::time_point t1, t2;
  std::chrono::duration<double> time_span;
#endif
  bool inUse = false;
  double time_ms;

 public:
  HighPerformanceTimer() : inUse(false)
  {
#if defined(_WIN32)
    QueryPerformanceFrequency(&cpuClockFreq);
#endif
    reset();
  }

  void start()
  {
    inUse = true;
#if defined(_WIN32)
    QueryPerformanceCounter(&beginClock);
#else
    t1 = std::chrono::high_resolution_clock::now();
#endif
  }

  void stop()
  {
    inUse = false;
#if defined(_WIN32)
    QueryPerformanceCounter(&endClock);
    wallTime_ticks.QuadPart += endClock.QuadPart - beginClock.QuadPart;
#else
    t2 = std::chrono::high_resolution_clock::now();
    time_span += std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
#endif
  }

  double milliseconds()
  {
    assert(!inUse);
#if defined(_WIN32)
    double wallTime_us =
        double(wallTime_ticks.QuadPart) * 1000.0 * 1000.0 / double(cpuClockFreq.QuadPart);
    time_ms = wallTime_us / 1000.0;
#else
    time_ms = time_span.count() * 1000.0f;
#endif
    return time_ms;
  }

  void reset()
  {
#if defined(_WIN32)
    wallTime_ticks.QuadPart = 0;
#else
    time_span = std::chrono::duration<double>(0);
#endif
  }

  void measureTime(const std::string &msg)
  {
    stop();
    milliseconds();
    if (time_ms < 1000) {
      printf("%s [time: %.3f ms]\n", msg.c_str(), time_ms);
    } else {
      printf("%s [time: %.3f s ]\n", msg.c_str(), time_ms / 1000);
    }
  }

  void measureBandwidth(size_t bytes, const std::string &msg)
  {
    stop();
    milliseconds();
    const double mb = double(bytes) / 1000.0 / 1000.0;
    const double time_s = time_ms / 1000.0;
    const double bwmb_s = mb / time_s;
    printf("%s (%.3f MB) [I/O: %.3f MB/s]\n", msg.c_str(), mb, bwmb_s);
  }

  template <typename... Ts>
  void measure(const std::string &format, Ts... rest)
  {
    measureTime(stringf(format, rest...));
  }

  template <typename... Ts>
  void measure(size_t bytes, const std::string &format, Ts... rest)
  {
    measureBandwidth(bytes, stringf(format, rest...));
  }

  void run(std::function<void()> func, const std::string &msg)
  {
    reset();
    start();
    func();
    measureTime(msg);
  }

  void run(std::function<void()> func, size_t bytes, const std::string &msg)
  {
    reset();
    start();
    func();
    measureBandwidth(bytes, msg);
  }

 private:
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
};

}  // namespace vidi

#endif  // VIDI_HIGH_PERFORMANCE_TIMER_H
