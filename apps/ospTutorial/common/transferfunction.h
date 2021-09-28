#pragma once

#ifndef V3D_UTIL_TRANSFERFUNCTION_H_
#define V3D_UTIL_TRANSFERFUNCTION_H_

#include "vidi_json.h"

#include <rkcommon/math/vec.h>

#include <cmath>
#include <memory>
#include <vector>

// #include "Util/IRgbaFunction.h"
// #include "Util/ILookupTable.h"
// #include "Util/Vector.h"

using namespace rkcommon::math;

namespace {

template <typename T1, typename T2>
inline T1 mix(const T1 &x, const T1 &y, const T2 &a)
{
  return (x + a * (y - x));
}

} // namespace

class TransferFunction /*: public ILookupTable<vec4>*/
{ // public IRgbaFunction {
 public:
  struct ColorControl
  {
    ColorControl() : position(0.0f), color(1.0f, 1.0f, 1.0f) {}
    ColorControl(float _value, float r, float g, float b)
        : position(_value), color(r, g, b)
    {}
    ColorControl(float _value, const vec3f &rgb) : position(_value), color(rgb)
    {}
    bool operator<(const ColorControl &other) const
    {
      return (position < other.position);
    }
    float position;
    vec3f color;
  };

  struct OpacityControl
  {
    OpacityControl() : pos(0.0) {}
    OpacityControl(const vec2f &pos_) : pos(pos_) {}
    vec2f pos;
  };

  // TODO: fix problem when stdev = 0

  struct GaussianObject
  {
    GaussianObject()
        : mean(0.5f), sigma(1.0f), heightFactor(1.0f), alphaArray(1024)
    {
      update();
    }
    GaussianObject(
        float _mean, float _sigma, float _heightFactor, int resolution)
        : mean(_mean),
          sigma(_sigma),
          heightFactor(_heightFactor),
          alphaArray(resolution)
    {
      update();
    }
    float value(float x) const
    {
      float diff = x - mean;
      return heightFactor / (sigma * std::sqrt(2.0f * float(M_PI)))
          * std::exp(-(diff * diff) / (2.0f * sigma * sigma));
    }
    float height() const
    {
      return value(mean);
    }
    void setHeight(float h)
    {
      heightFactor = h * sigma * std::sqrt(2.0f * float(M_PI));
    }
    void update()
    {
      float invRes = 1.0f / float(alphaArray.size());
      for (size_t i = 0; i < alphaArray.size(); ++i) {
        float val = value((float(i) + 0.5f) * invRes);
        alphaArray[i] = clamp(val, 0.0f, 1.0f);
      }
    }
    float mean;
    float sigma;
    float heightFactor;
    std::vector<float> alphaArray;
  };

 public:
  TransferFunction(int resolution = 1024);
  TransferFunction(const TransferFunction &other);
  ~TransferFunction();

  //    TransferFunction& operator=(const TransferFunction& other);     // deep
  //    copy

  TransferFunction &operator=(const TransferFunction &) = default; // deep copy

  void clear();

  //    const std::vector<vec4>& rgbaTable() const override { return _rgbaTable;
  //    }

  const vec4f *data() const
  {
    return _rgbaTable.data();
  }
  int size() const
  {
    return int(_rgbaTable.size());
  }

  int resolution() const
  {
    return int(_rgbaTable.size());
  }
  float *alphaArray()
  {
    return &_alphaArray.front();
  }
  const float *alphaArray() const
  {
    return &_alphaArray.front();
  }

  void clearAlphaTable();

  int colorControlCount() const
  {
    return int(_colorControls.size());
  }
  ColorControl &colorControl(int index)
  {
    return _colorControls[index];
  }
  const ColorControl &colorControl(int index) const
  {
    return _colorControls[index];
  }
  ColorControl &addColorControl(const ColorControl &control);
  ColorControl &addColorControl(float value, float r, float g, float b);
  ColorControl &insertColorControl(float pos);
  void removeColorControl(int index);
  void clearColorControls();

  int opacityControlCount() const
  {
    return int(_opacityControls.size());
  }
  OpacityControl &opacityControl(int index)
  {
    return _opacityControls[index];
  }
  const OpacityControl &opacityControl(int index) const
  {
    return _opacityControls[index];
  }
  OpacityControl &addOpacityControl(const OpacityControl &ctrl);
  OpacityControl &addOpacityControl(const vec2f &pos);
  void removeOpacityControl(int index);
  void clearOpacityControls();

  int gaussianObjectCount() const
  {
    return int(_gaussianObjects.size());
  }
  GaussianObject &gaussianObject(int index)
  {
    return _gaussianObjects[index];
  }
  const GaussianObject &gaussianObject(int index) const
  {
    return _gaussianObjects[index];
  }
  GaussianObject &addGaussianObject(const GaussianObject &gaussObj);
  GaussianObject &addGaussianObject(
      float mean, float sigma, float heightFactor);
  void removeGaussianObject(int index);
  void clearGaussianObjects();

  void updateColorMap();

  bool load(const char *fileName);
  bool save(const char *fileName) const;

  static std::unique_ptr<TransferFunction> fromRainbowMap(
      int resolution = 1024);

 protected:
  void updateFromOpacityControls();

 private:
  std::vector<vec4f> _rgbaTable;
  std::vector<float> _alphaArray; // alpha
  int _blendMode; // TODO(Min): implement different blend modes
  std::vector<ColorControl> _colorControls; // color control points
  std::vector<OpacityControl> _opacityControls;
  std::vector<GaussianObject> _gaussianObjects;
};

inline TransferFunction::TransferFunction(int resolution)
    : _rgbaTable(resolution), _alphaArray(resolution, 0), _blendMode(0)
{
  assert(resolution > 0);

  _colorControls.push_back(ColorControl(0.0f, 0.0f, 0.0f, 0.0f));
  _colorControls.push_back(ColorControl(1.0f, 1.0f, 1.0f, 1.0f));

  GaussianObject gaussObj = GaussianObject(0.5f, 0.1f, 1.0f, resolution);
  gaussObj.setHeight(0.5f);
  gaussObj.update();
  for (int i = 0; i < resolution; ++i)
    _alphaArray[i] = gaussObj.alphaArray[i];

  updateColorMap();
}

inline TransferFunction::TransferFunction(const TransferFunction &other)
{
  *this = other;
}

inline TransferFunction::~TransferFunction() {}

// TODO: remove this
// TransferFunction& TransferFunction::operator=(const TransferFunction& other)
// {
//    if (this == &other)
//        return *this;
//    _rgbaTable = other._rgbaTable;
//    _alphaArray = other._alphaArray;
//    _blendMode = other._blendMode;
//    _colorControls = other._colorControls;
//    _gaussianObjects = other._gaussianObjects;
//    return *this;
//}

inline void TransferFunction::clear()
{
  _rgbaTable.clear();
  _alphaArray.clear();
  _colorControls.clear();
  _opacityControls.clear();
  _gaussianObjects.clear();
}

inline void TransferFunction::clearAlphaTable()
{
  for (int i = 0; i < int(_alphaArray.size()); ++i)
    _alphaArray[i] = 0.0f;
  updateColorMap();
}

inline TransferFunction::ColorControl &TransferFunction::addColorControl(
    const ColorControl &control)
{
  _colorControls.push_back(control);
  updateColorMap();
  return _colorControls.back();
}

inline TransferFunction::ColorControl &TransferFunction::addColorControl(
    float value, float r, float g, float b)
{
  return addColorControl(ColorControl(value, r, g, b));
}

inline TransferFunction::ColorControl &TransferFunction::insertColorControl(
    float pos)
{
  ColorControl control;
  control.position = pos;

  std::vector<ColorControl> colorControls = _colorControls;
  std::sort(colorControls.begin(), colorControls.end());
  int controlCount = int(colorControls.size());

  // find the first color control greater than the value
  int firstLarger = 0;
  while (
      firstLarger < controlCount && pos > colorControls[firstLarger].position)
    firstLarger++;

  if (firstLarger <= 0) { // less than the leftmost color control
    control.color = colorControls[firstLarger].color;
  } else if (firstLarger
      >= controlCount) { // greater than the rightmost color control
    control.color = colorControls[firstLarger - 1].color;
  } else { // between two color controls
    ColorControl &left = colorControls[firstLarger - 1];
    ColorControl &right = colorControls[firstLarger];
    float w = std::abs(pos - left.position)
        / std::abs(right.position - left.position);
    control.color = mix(left.color, right.color, w);
  }

  _colorControls.push_back(control);
  updateColorMap();
  return _colorControls.back();
}

inline void TransferFunction::removeColorControl(int index)
{
  assert(index >= 0 && index < int(_colorControls.size()));
  for (int i = index; i < int(_colorControls.size()) - 1; ++i)
    _colorControls[i] = _colorControls[i + 1];
  _colorControls.pop_back();
  updateColorMap();
}

inline void TransferFunction::clearColorControls()
{
  _colorControls.clear();
  updateColorMap();
}

inline TransferFunction::OpacityControl &TransferFunction::addOpacityControl(
    const OpacityControl &ctrl)
{
  _opacityControls.push_back(ctrl);
  updateColorMap();
  return _opacityControls.back();
}

inline TransferFunction::OpacityControl &TransferFunction::addOpacityControl(
    const vec2f &pos)
{
  return addOpacityControl(OpacityControl(pos));
}

inline void TransferFunction::removeOpacityControl(int index)
{
  assert(index >= 0 && index < int(_opacityControls.size()));
  for (int i = index; i < int(_opacityControls.size()) - 1; ++i)
    _opacityControls[i] = _opacityControls[i + 1];
  _opacityControls.pop_back();
  updateColorMap();
}

inline void TransferFunction::clearOpacityControls()
{
  _opacityControls.clear();
  updateColorMap();
}

inline TransferFunction::GaussianObject &TransferFunction::addGaussianObject(
    const GaussianObject &gaussObj)
{
  _gaussianObjects.push_back(gaussObj);
  if (_gaussianObjects.back().alphaArray.size() != _alphaArray.size()) {
    _gaussianObjects.back().alphaArray.resize(_alphaArray.size());
    _gaussianObjects.back().update();
  }
  updateColorMap();
  return _gaussianObjects.back();
}

inline TransferFunction::GaussianObject &TransferFunction::addGaussianObject(
    float mean, float sigma, float heightFactor)
{
  _gaussianObjects.push_back(
      GaussianObject(mean, sigma, heightFactor, resolution()));
  updateColorMap();
  return _gaussianObjects.back();
}

inline void TransferFunction::removeGaussianObject(int index)
{
  assert(index >= 0 && index < int(_gaussianObjects.size()));
  for (int i = index; i < int(_gaussianObjects.size()) - 1; ++i)
    _gaussianObjects[i] = _gaussianObjects[i + 1];
  _gaussianObjects.pop_back();
  updateColorMap();
}

inline void TransferFunction::clearGaussianObjects()
{
  _gaussianObjects.clear();
  updateColorMap();
}

inline void TransferFunction::updateColorMap()
{
  //    if (_colorControls.size() < 1)
  //        return;                     // no valid color map

  std::vector<ColorControl> colorControls = _colorControls;

  if (colorControls.empty()) {
    colorControls.push_back(ColorControl(0.0f, 0.0f, 0.0f, 0.0f));
  }

  std::sort(colorControls.begin(), colorControls.end());
  int controlCount = int(colorControls.size());
  //    int firstLarger = 0;

  auto upperBound = colorControls.begin();

  for (int i = 0; i < resolution(); ++i) {
    float value = (float(i) + 0.5f) / float(resolution());
    vec3f color;

    // find the first color control greater than the value
    //        while (firstLarger < controlCount && value >
    //        colorControls[firstLarger].position)
    //            firstLarger++;

    upperBound = std::upper_bound(
        upperBound, colorControls.end(), ColorControl(value, 0.0f, 0.0f, 0.0f));

    int ubIndex = int(upperBound - colorControls.begin());

    //        if (firstLarger != upperBoundIndex) {
    //            std::cout << "[debug] not equal!" << std::endl;
    //        }

    if (ubIndex <= 0) { // less than the leftmost color control
      color = colorControls[ubIndex].color;
    } else if (ubIndex
        >= controlCount) { // greater than the rightmost color control
      color = colorControls[ubIndex - 1].color;
    } else { // between two color controls
      ColorControl &left = colorControls[ubIndex - 1];
      ColorControl &right = colorControls[ubIndex];
      float w = std::abs(value - left.position)
          / std::abs(right.position - left.position);
      color = mix(left.color, right.color, w);
    }

    _rgbaTable[i] = vec4f(color, _alphaArray[i]);

    //        if (_resolution == _arraySize) {
    //            _rgbaTable[i].a = _alphaArray[i];
    //        } else if (_resolution * 2 == _arraySize) {         //
    //            _rgbaTable[i].a = _alphaArray[i * 2];           // nearest -
    //            VisKit
    //        } else  {
    //            // TODO: verify this

    //            // sample from alpha array
    //            int f = int(floor(value * _arraySize - 0.5f));
    //            int c = f + 1;
    //            f = max(f, 0);
    //            c = min(c, _arraySize - 1);
    //            float w = max(value * _arraySize - (int(f) + 0.5f), 0.0f);
    //            _rgbaTable[i].a = lerp(_alphaArray[f], _alphaArray[c], w);
    //        }

    for (int j = 0; j < int(_gaussianObjects.size()); ++j) {
      _rgbaTable[i].w = max(_rgbaTable[i].w, _gaussianObjects[j].alphaArray[i]);
      //            if (_rgbaTable[i].a < _gaussianObjects[j].alphaArray[i]) //
      //            maximum
      //                _rgbaTable[i].a = _gaussianObjects[j].alphaArray[i];
    }
  }

  updateFromOpacityControls();
}

inline void TransferFunction::updateFromOpacityControls()
{
  if (_opacityControls.empty())
    return;
  std::vector<OpacityControl> opcControls = _opacityControls;
  auto compFunc = [](const OpacityControl &a, const OpacityControl &b) {
    return (a.pos.x < b.pos.x);
  };
  std::sort(opcControls.begin(), opcControls.end(), compFunc);
  int controlCount = int(opcControls.size());
  auto upperBound = opcControls.begin();
  for (int i = 0; i < resolution(); ++i) {
    double value = (double(i) + 0.5f) / double(resolution());
    upperBound = std::upper_bound(upperBound,
        opcControls.end(),
        OpacityControl(vec2f(value, 0.0)),
        compFunc);
    int ubIndex = int(upperBound - opcControls.begin());
    double alpha;
    if (ubIndex <= 0) { // less than the leftmost control
      alpha = opcControls[ubIndex].pos.y;
    } else if (ubIndex >= controlCount) { // greater than the rightmost control
      alpha = opcControls[ubIndex - 1].pos.y;
    } else { // between two color controls
      auto &left = opcControls[ubIndex - 1];
      auto &right = opcControls[ubIndex];
      double w =
          std::abs(value - left.pos.x) / std::abs(right.pos.x - left.pos.x);
      alpha = mix(left.pos.y, right.pos.y, w);
    }
    _rgbaTable[i].w = max(_rgbaTable[i].w, float(alpha));
  }
}

// TODO: implement move() and return object directly instead of pointer
// static
inline std::unique_ptr<TransferFunction> TransferFunction::fromRainbowMap(
    int resolution)
{
  std::unique_ptr<TransferFunction> ret(new TransferFunction(resolution));

  ret->_colorControls.clear();
  ret->_colorControls.push_back(
      ColorControl(0.0f / 6.0f, 0.0f, 0.364706f, 1.0f));
  ret->_colorControls.push_back(
      ColorControl(1.0f / 6.0f, 0.0f, 1.0f, 0.976471f));
  ret->_colorControls.push_back(
      ColorControl(2.0f / 6.0f, 0.0f, 1.0f, 0.105882f));
  ret->_colorControls.push_back(
      ColorControl(3.0f / 6.0f, 0.968627f, 1.0f, 0.0f));
  ret->_colorControls.push_back(
      ColorControl(4.0f / 6.0f, 1.0f, 0.490196f, 0.0f));
  ret->_colorControls.push_back(ColorControl(5.0f / 6.0f, 1.0f, 0.0f, 0.0f));
  ret->_colorControls.push_back(
      ColorControl(6.0f / 6.0f, 0.662745f, 0.0f, 1.0f));

  ret->_gaussianObjects.clear();

  ret->updateColorMap();
  return ret;
}

namespace {

// std::string getSuffix(const std::string& filePath) {
//    size_t dotPos = filePath.find_last_of(".");
//    return (dotPos == std::string::npos) ? "" : filePath.substr(dotPos + 1);
//}

}

inline bool TransferFunction::load(const char * /*fileName*/)
{
  //    std::string fname(fileName);
  //    std::string suffix = getSuffix(fname);

  return false;
}

inline bool TransferFunction::save(const char * /*fileName*/) const
{
  //    std::string fname(fileName);
  //    std::string suffix = getSuffix(fname);

  return false;
}

inline vec3f colorFromJson(const vidi::json &js)
{
  if (!js.contains("r") || !js.contains("g") || !js.contains("b"))
    return vec3f();
  return vec3f(
      js["r"].get<float>(), js["g"].get<float>(), js["b"].get<float>());
}

template <>
inline TransferFunction get_from_json<TransferFunction>(const vidi::json &js)
{
  int resolution = 1024;
  if (js.contains("resolution")) {
    resolution = js["resolution"].get<int>();
  }

  std::string alphaArrayBase64;
  if (js.contains("alphaArray") && js["alphaArray"].contains("data")) {
    assert("BASE64" == js["alphaArray"]["encoding"].get<std::string>());
    alphaArrayBase64 = js["alphaArray"]["data"].get<std::string>();
    resolution = int(vidi::size_base64(alphaArrayBase64) / sizeof(float));
  }

  TransferFunction tf(resolution);
  if (!alphaArrayBase64.empty()) {
    vidi::from_base64(
        alphaArrayBase64, reinterpret_cast<char *>(tf.alphaArray()));
  }

  //    vec4 bgColor = Serializer::toVector4f(value["backgroundColor"]);
  //    tf.setBackgroundColor(bgColor.x, bgColor.y, bgColor.z);

  if (js.contains("colorControls")) {
    tf.clearColorControls();
    int count = js["colorControls"].size();
    for (int i = 0; i < count; ++i) {
      const vidi::json &jsonCC = js["colorControls"][i];
      if (!jsonCC.contains("position") || !jsonCC.contains("color"))
        continue;
      TransferFunction::ColorControl colorControl;
      colorControl.position = jsonCC["position"].get<float>();
      colorControl.color = colorFromJson(jsonCC["color"]);
      tf.addColorControl(colorControl);
    }
  }

  tf.clearOpacityControls();
  if (js.contains("opacityControl")) {
    int count = js["opacityControl"].size();
    for (int i = 0; i < count; ++i) {
      const vidi::json &jsonOC = js["opacityControl"][i];
      if (!jsonOC.contains("position"))
        continue;
      TransferFunction::OpacityControl opcControl(
          vec2f(jsonOC["position"]["x"].get<float>(),
              jsonOC["position"]["y"].get<float>()));
      tf.addOpacityControl(opcControl);
    }
  }

  tf.clearGaussianObjects();
  if (js.contains("gaussianObjects")) {
    int count = js["gaussianObjects"].size();
    for (int i = 0; i < count; ++i) {
      const vidi::json &jsonGO = js["gaussianObjects"][i];
      if (!jsonGO.contains("mean") || !jsonGO.contains("sigma")
          || !jsonGO.contains("heightFactor"))
        continue;
      TransferFunction::GaussianObject gaussianObject(
          jsonGO["mean"].get<float>(),
          jsonGO["sigma"].get<float>(),
          jsonGO["heightFactor"].get<float>(),
          resolution);
      tf.addGaussianObject(gaussianObject);
    }
  }

  tf.updateColorMap();

  return tf;
}

#endif // V3D_UTIL_TRANSFERFUNCTION_H_
