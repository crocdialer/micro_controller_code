#pragma once

#include "Arduino.h"

template <typename T>
const char* fmt_real_to_str(T val, uint32_t precision = 3)
{
    static char ret[32], fmt_buf[32];
    int multiplier = 1;
    for(int i = 0; i < precision; i++){ multiplier *= 10; }
    T frac = val - int(val);
    int32_t fmt = frac * multiplier;
    sprintf(fmt_buf, "%%d.%%%02dd", (int)precision);
    sprintf(ret, fmt_buf, (int)val, fmt);
    return ret;
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
