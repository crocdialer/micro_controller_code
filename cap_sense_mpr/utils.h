#pragma once

#include "Arduino.h"

template <typename T>
void fmt_real_to_str(char *buf, T val, uint32_t precision = 3)
{
    int multiplier = 1;
    char fmt_string[8];
    // sprintf(fmt_string, "%d.%03d")
    for(int i = 0; i < precision; i++){ multiplier *= 10; }
    T frac = val - (int)val;
    frac = frac < 0 ? -frac : frac;
    int32_t fmt = frac * multiplier;
    sprintf(buf, "%d.%03d", (int)val, fmt);
};

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
  float length2() const {
    return x * x + y * y + z * z;
  }
  float length() const{
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
};

template<typename T>
class CircularBuffer
{
public:

    CircularBuffer(uint32_t the_cap = 10):
    m_array_size(0),
    m_first(0),
    m_last(0),
    m_data(NULL)
    {
        set_capacity(the_cap);
    }

    virtual ~CircularBuffer()
    {
        if(m_data){ delete[](m_data); }
    }

    inline void clear()
    {
        m_first = m_last = 0;
    }

    inline void push(const T &the_val)
    {
        m_data[m_last] = the_val;
        m_last = (m_last + 1) % m_array_size;

        if(m_first == m_last){ m_first = (m_first + 1) % m_array_size; }
    }

    inline const T pop()
    {
        if(!empty())
        {
            const T ret = m_data[m_first];
            m_first = (m_first + 1) % m_array_size;
            return ret;
        }
        else{ return T(0); }
    }

    inline uint32_t capacity() const { return m_array_size - 1; };
    void set_capacity(uint32_t the_cap)
    {
        if(m_data){ delete[](m_data); }
        m_data = new T[the_cap + 1];
        m_array_size = the_cap + 1;
        clear();
    }

    inline uint32_t size() const
    {
        int ret = m_last - m_first;
        if(ret < 0){ ret += m_array_size; }
        return ret;
    };

    inline bool empty() const { return m_first == m_last; }

    inline const T operator[](uint32_t the_index) const
    {
        if(the_index < size()){ return m_data[(m_first + the_index) % m_array_size]; }
        else{ return T(0); }
    };

private:

    int32_t m_array_size, m_first, m_last;
    T* m_data;
};
