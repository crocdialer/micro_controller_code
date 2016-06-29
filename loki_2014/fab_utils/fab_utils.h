#pragma once

#include "Arduino.h"

template <typename T>
inline int sgn(T val)
{
    return (T(0) < val) - (val < T(0));
}

template <typename T>
inline T random(const T &min, const T &max)
{
    return min + (max - min) * (rand() / (float) RAND_MAX);
}

template <typename T>
inline const T& clamp(const T &val, const T &min, const T &max)
{
    return val < min ? min : (val > max ? max : val);
}

template <typename T>
inline T mix(const T &lhs, const T &rhs, float ratio)
{
    return lhs + ratio * (rhs - lhs);
}

template <typename T>
inline T map_value(const T &val, const T &src_min, const T &src_max,
                   const T &dst_min, const T &dst_max)
{
    float mix_val = clamp<float>(val / (src_max - src_min), 0.f, 1.f);
    return mix<T>(dst_min, dst_max, mix_val);
}

class vec3 {
public:
  float x, y, z;  
  vec3():
  x(0), y(0), z(0) {
  }
  vec3(float x, float y, float z) :
  x(x), y(y), z(z) {
  }
  void set(const float* v) {
    x = v[0], y = v[1], z = v[2];
  }
  void set(float vx, float vy, float vz) {
    x = vx, y = vy, z = vz;
  }
  void set(const vec3& v) {
    x = v.x, y = v.y, z = v.z;
  }
  void zero() {
    x = 0, y = 0, z = 0;
  }
  float length2() {
    return x * x + y * y + z * z;
  }
  float length() {
    return sqrt(x * x + y * y + z * z);
  }
  boolean operator==(const vec3& v) {
    return x == v.x && y == v.y && z == v.z;
  }
  boolean operator!=(const vec3& v) {
    return x != v.x || y != v.y || z != v.z;
  }
  void operator+=(const vec3& v) {
    x += v.x, y += v.y, z += v.z;
  }
  void operator-=(const vec3& v) {
    x -= v.x, y -= v.y, z -= v.z;
  }
  void operator/=(float v) {
    x /= v, y /= v, z /= v;
  }
  void operator*=(float v) {
    x *= v, y *= v, z *= v;
  }
  vec3 operator+(const vec3& v) {
    vec3 c = *this; 
    c += v; 
    return c;
  }
  vec3 operator-(const vec3& v) {
    vec3 c = *this; 
    c -= v; 
    return c;
  }
  vec3 operator/(float v) {
    vec3 c = *this; 
    c /= v; 
    return c;
  }
  vec3 operator*(float v) {
    vec3 c = *this; 
    c *= v; 
    return c;
  }
  operator float*() {
    return (float*) this;
  }
    
  float length() const {
    return sqrt(x*x + y*y + z*z);
  }
};
