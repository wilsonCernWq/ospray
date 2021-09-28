#pragma once

#include "vidi_json.h"

#include <rkcommon/math/vec.h>
#include <rkcommon/tasking/parallel_for.h>

#include <ospray/ospray.h>
#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/rkcommon.h>

#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>

#include <fstream>

NLOHMANN_JSON_SERIALIZE_ENUM(OSPDataType,
    {
        {OSP_UCHAR, "UNSIGNED_BYTE"},
        {OSP_USHORT, "UNSIGNED_SHORT"},
        {OSP_UINT, "UNSIGNED_INT"},
        {OSP_CHAR, "BYTE"},
        {OSP_SHORT, "SHORT"},
        {OSP_INT, "INT"},
        {OSP_FLOAT, "FLOAT"},
        {OSP_DOUBLE, "DOUBLE"},
    });

using namespace rkcommon::math;

#define define_vector_serialization(T)                                         \
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vec2##T, x, y);                           \
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vec3##T, x, y, z);                        \
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vec4##T, x, y, z, w);

define_vector_serialization(i);
define_vector_serialization(f);
define_vector_serialization(d);

template <typename T>
T get_from_json(const vidi::json &js)
{
  T v;
  from_json(js, v);
  return v;
}

template <>
std::string get_from_json<std::string>(const vidi::json &js)
{
  return js.get<std::string>();
}

template <>
float get_from_json<float>(const vidi::json &js)
{
  return js.get<float>();
}

template <>
double get_from_json<double>(const vidi::json &js)
{
  return js.get<double>();
}

template <>
int get_from_json<int>(const vidi::json &js)
{
  return js.get<int>();
}

// ------------------------------------------------------------------
// I/O helper functions
// ------------------------------------------------------------------

template <size_t Size>
inline void swapBytes(void *data)
{
  char *p = reinterpret_cast<char *>(data);
  char *q = p + Size - 1;
  while (p < q)
    std::swap(*(p++), *(q--));
}

template <>
inline void swapBytes<1>(void *)
{}

template <>
inline void swapBytes<2>(void *data)
{
  char *p = reinterpret_cast<char *>(data);
  std::swap(p[0], p[1]);
}

template <>
inline void swapBytes<4>(void *data)
{
  char *p = reinterpret_cast<char *>(data);
  std::swap(p[0], p[3]);
  std::swap(p[1], p[2]);
}

template <>
inline void swapBytes<8>(void *data)
{
  char *p = reinterpret_cast<char *>(data);
  std::swap(p[0], p[7]);
  std::swap(p[1], p[6]);
  std::swap(p[2], p[5]);
  std::swap(p[3], p[4]);
}

template <typename T>
inline void swapBytes(T *data)
{
  swapBytes<sizeof(T)>(reinterpret_cast<void *>(data));
}

inline void reverseByteOrder(char *data, size_t elemCount, size_t elemSize)
{
  switch (elemSize) {
  case 1:
    break;
  case 2:
    for (size_t i = 0; i < elemCount; ++i)
      swapBytes<2>(&data[i * elemSize]);
    break;
  case 4:
    for (size_t i = 0; i < elemCount; ++i)
      swapBytes<4>(&data[i * elemSize]);
    break;
  case 8:
    for (size_t i = 0; i < elemCount; ++i)
      swapBytes<8>(&data[i * elemSize]);
    break;
  default:
    assert(false);
  }
}

template <typename T>
inline vec2f compute_minmax(void *_array, size_t count)
{
  T *array = (T *)_array;

  float actual_max = tbb::parallel_reduce(
      tbb::blocked_range<T *>(array, array + count),
      -std::numeric_limits<float>::max(),
      [](const tbb::blocked_range<T *> &r, float init) -> float {
        for (T *a = r.begin(); a != r.end(); ++a)
          init = std::max(init, static_cast<float>(*a));
        return init;
      },
      [](float x, float y) -> float { return std::max(x, y); });

  float actual_min = tbb::parallel_reduce(
      tbb::blocked_range<T *>(array, array + count),
      std::numeric_limits<float>::max(),
      [](const tbb::blocked_range<T *> &r, float init) -> float {
        for (T *a = r.begin(); a != r.end(); ++a)
          init = std::min(init, static_cast<float>(*a));
        return init;
      },
      [](float x, float y) -> float { return std::min(x, y); });

  return vec2f(actual_min, actual_max);
}

inline vec2f compute_minmax(void *array, size_t count, OSPDataType type)
{
  switch (type) {
  case OSP_UCHAR:
    return compute_minmax<uint8_t>(array, count);
  case OSP_CHAR:
    return compute_minmax<int8_t>(array, count);
  case OSP_USHORT:
    return compute_minmax<uint16_t>(array, count);
  case OSP_SHORT:
    return compute_minmax<int16_t>(array, count);
  case OSP_UINT:
    return compute_minmax<uint32_t>(array, count);
  case OSP_INT:
    return compute_minmax<int32_t>(array, count);
  case OSP_FLOAT:
    return compute_minmax<float>(array, count);
  case OSP_DOUBLE:
    return compute_minmax<double>(array, count);
  default:
    throw std::runtime_error("#osp: unexpected volume type ...");
  }
}

inline std::pair<ospray::cpp::Volume, std::shared_ptr<char[]>>
RegularVolumeReader(const std::string &filename,
    const OSPDataType data_type,
    const vec3i data_dimensions,
    const size_t data_offset,
    const bool data_is_big_endian)
{
  assert(data_dimensions.x > 0);
  assert(data_dimensions.y > 0);
  assert(data_dimensions.z > 0);

  ospray::cpp::SharedData data;

  const size_t data_elem_count =
      (size_t)data_dimensions.x * data_dimensions.y * data_dimensions.z;

  const size_t data_elem_size = // clang-format off
      (data_type == OSP_UCHAR  || data_type == OSP_CHAR)  ? sizeof(uint8_t)  :
      (data_type == OSP_USHORT || data_type == OSP_SHORT) ? sizeof(uint16_t) :
      (data_type == OSP_UINT   || data_type == OSP_INT)   ? sizeof(uint32_t) :
      (data_type == OSP_FLOAT) ? sizeof(float) : sizeof(double); // clang-format on

  const size_t data_size = data_elem_count * data_elem_size;

  std::shared_ptr<char[]> data_ptr;

  std::ifstream ifs(filename, std::ios::in | std::ios::binary);
  if (ifs.fail()) {
    throw std::runtime_error("Cannot open the file");
  }

  ifs.seekg(0, std::ios::end);
  size_t file_size = ifs.tellg();
  if (file_size < data_offset + data_size) {
    throw std::runtime_error("File size does not match data size");
  }
  ifs.seekg(data_offset, std::ios::beg);

  try {
    data_ptr.reset(new char[data_size]);
  } catch (std::bad_alloc &) {
    throw std::runtime_error("Cannot allocate memory for the data");
  }

  ifs.read(data_ptr.get(), data_size);
  if (ifs.fail()) {
    throw std::runtime_error("Cannot read the file");
  }

  // reverse byte-order if necessary
  const bool reverse = (data_is_big_endian && data_elem_size > 1);
  if (reverse) {
    reverseByteOrder(data_ptr.get(), data_elem_count, data_elem_size);
  }

  ifs.close();

  switch (data_type) {
  case OSP_UCHAR:
    data = ospray::cpp::SharedData(
        reinterpret_cast<unsigned char *>(data_ptr.get()), data_dimensions);
    break;
  case OSP_CHAR:
    data = ospray::cpp::SharedData(
        reinterpret_cast<char *>(data_ptr.get()), data_dimensions);
    break;
  case OSP_USHORT:
    data = ospray::cpp::SharedData(
        reinterpret_cast<unsigned short *>(data_ptr.get()), data_dimensions);
    break;
  case OSP_SHORT:
    data = ospray::cpp::SharedData(
        reinterpret_cast<short *>(data_ptr.get()), data_dimensions);
    break;
  case OSP_UINT:
    data = ospray::cpp::SharedData(
        reinterpret_cast<unsigned int *>(data_ptr.get()), data_dimensions);
    break;
  case OSP_INT:
    data = ospray::cpp::SharedData(
        reinterpret_cast<int *>(data_ptr.get()), data_dimensions);
    break;
  case OSP_FLOAT:
    data = ospray::cpp::SharedData(
        reinterpret_cast<float *>(data_ptr.get()), data_dimensions);
    break;
  case OSP_DOUBLE:
    data = ospray::cpp::SharedData(
        reinterpret_cast<double *>(data_ptr.get()), data_dimensions);
    break;
  default:
    throw std::runtime_error("#osp: unexpected volume type ...");
  }

  data.commit();

  // setuo the ospray volume
  ospray::cpp::Volume volume("structuredRegular");
  volume.setParam("data", data);
  volume.setParam("gridOrigin", vec3f(0.f));
  volume.setParam("gridSpacing", vec3f(1.f));
  volume.commit();

  return std::make_pair(volume, data_ptr);
}
