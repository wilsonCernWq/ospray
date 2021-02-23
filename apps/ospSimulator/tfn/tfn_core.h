// ======================================================================== //
// Copyright Qi Wu, since 2019                                              //
// Copyright SCI Institute, University of Utah, 2018                        //
// ======================================================================== //
#pragma once

#define TFN_MODULE_INTERFACE // place-holder

#include <array>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tfn {

// ========================================================================
#define TFN_MODULE_VERSION "0.02 WIP"
inline TFN_MODULE_INTERFACE const char *GetVersion()
{
  return TFN_MODULE_VERSION;
  return TFN_MODULE_VERSION;
  return TFN_MODULE_VERSION;
};

// ========================================================================
// The magic number is 'OSTF' in ASCII
const static uint32_t MAGIC_NUMBER = 0x4f535446;
const static uint64_t CURRENT_VERSION = 1;

#ifdef TFN_MODULE_EXTERNAL_VECTOR_TYPES
/*! we assume the app already defines osp::vec types. Note those
  HAVE to be compatible with the data layouts used below.
  Note: this feature allows the application to use its own vector
  type library in the following way
  a) include your own vector library (say, ospcommon::vec3f etc, when using
     the ospcommon library)
  b) make sure the proper vec3f etc are defined in a osp:: namespace, e.g.,
     using
     namespace tfn {
       typedef ospcommon::vec3f vec3f;
     }
  c) defines OSPRAY_EXTERNAL_VECTOR_TYPES
  d) include vec.h
  ! */
#else
// clang-format off
struct vec2f { float x, y; };
struct vec2i { int x, y; };
struct vec3f { float x, y, z; };
struct vec3i { int x, y, z; };
struct vec4f { float x, y, z, w; };
struct vec4i { int x, y, z, w; };
// clang-format on
#endif

using list1f = std::vector<float>;
using list2f = std::vector<vec2f>;
using list3f = std::vector<vec3f>;

/* The transfer function file format used by the OSPRay sample apps is a
 * little endian binary format with the following layout:
 *
 * VERSION 1:
 *
 * uint32: magic number identifying the file
 * uint64: version number
 * uint64: length of the name of the transfer function (not including \0)
 * [char...]: name of the transfer function (without \0)
 * uint64: number of vec3f color values
 * uint64: numer of vec2f data value, opacity value pairs
 * float64: data value min
 * float64: data value max
 * float32: opacity scaling value, opacity values should be scaled
 *          by this factor
 * [vec3f...]: RGB values
 * [vec2f...]: data value, opacity value pairs
 */

// ========================================================================
struct TFN_MODULE_INTERFACE ColorPoint
{
  float p; // Location of the control point [0, 1]
  float r, g, b;
  ColorPoint() = default;
  ColorPoint(const float cp, const float cr, const float cg, const float cb);
  ColorPoint(const float cp, const vec3f &r);
  unsigned long GetHex(); // This function gives Hex color for ImGui
};

inline ColorPoint::ColorPoint(const float cp, const float cr, const float cg, const float cb) : p(cp), r(cr), g(cg), b(cb) {}

inline ColorPoint::ColorPoint(const float cp, const vec3f &r) : p(cp), r(r.x), g(r.y), b(r.z) {}

/* This function gives Hex color for ImGui */
inline unsigned long ColorPoint::GetHex()
{
  return (0xff << 24) + ((static_cast<uint8_t>(b * 255.f) & 0xff) << 16) + ((static_cast<uint8_t>(g * 255.f) & 0xff) << 8)
      + ((static_cast<uint8_t>(r * 255.f) & 0xff));
}

// ========================================================================
struct TFN_MODULE_INTERFACE OpacityPoint_Linear
{
  float p; // location of the control point [0, 1]
  float a;
  OpacityPoint_Linear() = default;
  OpacityPoint_Linear(const float cp, const float ca);
};

inline OpacityPoint_Linear::OpacityPoint_Linear(const float cp, const float ca) : p(cp), a(ca) {}

// ========================================================================
#if 0
struct TFN_MODULE_INTERFACE OpacityPoint_Gaussian
{
  float p; // location of the control point [0, 1]
  float a;
  float sw, sy; // width/height of the gaussian function
  float bx, by;
  OpacityPoint_Gaussian() = default;
  OpacityPoint_Gaussian(const float cp, const float ca, const float csw, const float csy, const float cbx, const float cby);
  OpacityPoint_Gaussian(const OpacityPoint_Gaussian &c);
  OpacityPoint_Gaussian &operator=(const OpacityPoint_Gaussian &c);
};

inline OpacityPoint_Gaussian::OpacityPoint_Gaussian(const float cp, const float ca, const float csw, const float csy, const float cbx, const float cby)
    : p(cp), a(ca), sw(csw), sy(csy), bx(cbx), by(cby)
{}

inline OpacityPoint_Gaussian::OpacityPoint_Gaussian(const OpacityPoint_Gaussian &c) : p(c.p), a(c.a), sw(c.sw), sy(c.sy), bx(c.bx), by(c.by) {}

inline OpacityPoint_Gaussian &OpacityPoint_Gaussian::operator=(const OpacityPoint_Gaussian &c)
{
  if (this == &c) {
    return *this;
  }
  p = c.p;
  a = c.a;
  sw = c.sw;
  sy = c.sy;
  bx = c.bx;
  by = c.by;
  return *this;
}
#endif

// ========================================================================
struct TFN_MODULE_INTERFACE TransferFunction
{
  std::string name;
  std::vector<tfn::vec3f> rgbValues;
  std::vector<tfn::vec2f> opacityValues;
  double dataValueMin;
  double dataValueMax;
  float opacityScaling;
  // Load the transfer function data in the file
  TransferFunction(const std::string &fileName);
  // Construct a transfer function from some existing one
  TransferFunction(const std::string &name,
      const std::vector<tfn::vec3f> &rgbValues,
      const std::vector<tfn::vec2f> &opacityValues,
      const double dataValueMin,
      const double dataValueMax,
      const float opacityScaling);
  // Load the transfer function data from a file
  void load(const std::string &fileName);
  // Save the transfer function data to the file
  void save(const std::string &fileName) const;
};

struct TFN_MODULE_INTERFACE TransferFunctionData
{
  bool editable;
  std::vector<ColorPoint> color_points;
  std::vector<OpacityPoint_Linear> opaticy_points;
  tfn::TransferFunction data;
};

inline TransferFunction::TransferFunction(const std::string &fileName)
{
  load(fileName);
}

inline TransferFunction::TransferFunction(const std::string &name,
    const std::vector<tfn::vec3f> &rgbValues,
    const std::vector<tfn::vec2f> &opacityValues,
    const double dataValueMin,
    const double dataValueMax,
    const float opacityScaling)
    : name(name), rgbValues(rgbValues), opacityValues(opacityValues), dataValueMin(dataValueMin), dataValueMax(dataValueMax), opacityScaling(opacityScaling)
{}

inline void TransferFunction::load(const std::string &fileName)
{
  std::ifstream fin(fileName.c_str(), std::ios::binary);
  if (!fin.is_open()) {
    throw std::runtime_error("File " + fileName + " not found");
  }
  // Verify this is actually a tfn::TransferFunction data file
  uint32_t magic = 0;
  if (!fin.read(reinterpret_cast<char *>(&magic), sizeof(uint32_t))) {
    throw std::runtime_error("Failed to read magic number header from " + fileName);
  }
  if (magic != MAGIC_NUMBER) {
    throw std::runtime_error("Read invalid identification header from " + fileName);
  }
  uint64_t version = 0;
  if (!fin.read(reinterpret_cast<char *>(&version), sizeof(uint64_t))) {
    throw std::runtime_error("Failed to read version header from " + fileName);
  }
  // Check if it's a supported version we can parse
  if (version != CURRENT_VERSION) {
    throw std::runtime_error("Got invalid version number from " + fileName);
  }
  uint64_t nameLen = 0;
  if (!fin.read(reinterpret_cast<char *>(&nameLen), sizeof(uint64_t))) {
    throw std::runtime_error("Failed to read nameLength header from " + fileName);
  }
  name.resize(nameLen);
  if (!fin.read(&name[0], nameLen)) {
    throw std::runtime_error("Failed to read name from " + fileName);
  }
  uint64_t numColors = 0;
  if (!fin.read(reinterpret_cast<char *>(&numColors), sizeof(uint64_t))) {
    throw std::runtime_error("Failed to read numColors header from " + fileName);
  }
  uint64_t numOpacities = 0;
  if (!fin.read(reinterpret_cast<char *>(&numOpacities), sizeof(uint64_t))) {
    throw std::runtime_error("Failed to read numOpacities header from " + fileName);
  }
  if (!fin.read(reinterpret_cast<char *>(&dataValueMin), sizeof(double))) {
    throw std::runtime_error("Failed to read dataValueMin header from " + fileName);
  }
  if (!fin.read(reinterpret_cast<char *>(&dataValueMax), sizeof(double))) {
    throw std::runtime_error("Failed to read dataValueMax header from " + fileName);
  }
  if (!fin.read(reinterpret_cast<char *>(&opacityScaling), sizeof(float))) {
    throw std::runtime_error("Failed to read opacityScaling header from " + fileName);
  }
  rgbValues.resize(numColors, tfn::vec3f{0.f, 0.f, 0.f});
  if (!fin.read(reinterpret_cast<char *>(rgbValues.data()), numColors * 3 * sizeof(float))) {
    throw std::runtime_error("Failed to read color values from " + fileName);
  }
  opacityValues.resize(numOpacities, tfn::vec2f{0.f, 0.f});
  if (!fin.read(reinterpret_cast<char *>(opacityValues.data()), numOpacities * 2 * sizeof(float))) {
    throw std::runtime_error("Failed to read opacity values from " + fileName);
  }
}

inline void TransferFunction::save(const std::string &fileName) const
{
  std::ofstream fout(fileName.c_str(), std::ios::binary);
  if (!fout.is_open()) {
    throw std::runtime_error("Failed to open " + fileName + " for writing");
  }
  if (!fout.write(reinterpret_cast<const char *>(&MAGIC_NUMBER), sizeof(uint32_t))) {
    throw std::runtime_error("Failed to write magic number header to " + fileName);
  }
  if (!fout.write(reinterpret_cast<const char *>(&CURRENT_VERSION), sizeof(uint64_t))) {
    throw std::runtime_error("Failed to write version header to " + fileName);
  }

  const uint64_t nameLen = name.size();
  if (!fout.write(reinterpret_cast<const char *>(&nameLen), sizeof(uint64_t))) {
    throw std::runtime_error("Failed to write nameLength header to " + fileName);
  }
  if (!fout.write(name.data(), nameLen)) {
    throw std::runtime_error("Failed to write name to " + fileName);
  }
  const uint64_t numColors = rgbValues.size();
  if (!fout.write(reinterpret_cast<const char *>(&numColors), sizeof(uint64_t))) {
    throw std::runtime_error("Failed to write numColors header to " + fileName);
  }
  const uint64_t numOpacities = opacityValues.size();
  if (!fout.write(reinterpret_cast<const char *>(&numOpacities), sizeof(uint64_t))) {
    throw std::runtime_error("Failed to write numOpacities header to " + fileName);
  }
  if (!fout.write(reinterpret_cast<const char *>(&dataValueMin), sizeof(double))) {
    throw std::runtime_error("Failed to write dataValueMin header to " + fileName);
  }
  if (!fout.write(reinterpret_cast<const char *>(&dataValueMax), sizeof(double))) {
    throw std::runtime_error("Failed to write dataValueMax header to " + fileName);
  }
  if (!fout.write(reinterpret_cast<const char *>(&opacityScaling), sizeof(float))) {
    throw std::runtime_error("Failed to write opacityScaling header to " + fileName);
  }
  if (!fout.write(reinterpret_cast<const char *>(rgbValues.data()), numColors * 3 * sizeof(float))) {
    throw std::runtime_error("Failed to write color values to " + fileName);
  }
  if (!fout.write(reinterpret_cast<const char *>(opacityValues.data()), numOpacities * 2 * sizeof(float))) {
    throw std::runtime_error("Failed to write opacity values to " + fileName);
  }
}

} // namespace tfn
