#pragma once

#include "emp-tool/emp-tool.h"
#include "utils.h"

namespace emp
{

// seems better to just use data arrays of long longs or similar
template <typename T> inline bool getBit(const T &x, uint i)
{
    assert(sizeof(T) * 8 > i);
    return (x >> i) & 1; //! what about arithmatic shift? maybe we don't overshift?
}

template <> inline bool getBit(const bool &x, uint i)
{
    assert(i == 0); // only bit retriebvable from bool
    return x;       //! what about arithmatic shift? maybe we don't overshift?
}

template <> inline bool getBit(const block &x, uint i)
{
    assert(128 > i);

    uint64_t *data = (uint64_t *)&x;
    // compiler should optimize the power of two divisions and mod
    return (data[i / 64] >> (i % 64)) & 1;
}
template <typename T>                    // todo could probably be written better, makign it work...
inline void setBit(T &x, uint i, bool b) //! I must have written the same code in like a billion places
{
    assert(sizeof(T) * 8 > i);
    T one = get_all_zeros_of_type<T>();
    memset(&one, (bool)(b ^ getBit(x, i)), 1); // itIs ^want = what we need to xor isIs by
    x ^= (one << i);
}
template <> inline void setBit(block &x, uint i, bool b)
{
    uint64_t *data = (uint64_t *)&x;
    uint64_t mask = 1ULL << (i % 64);
    data[i / 64] = (data[i / 64] | mask) ^ (mask * !b);
}

template <typename T> inline bool getBitArr(const T *x, uint i)
{
    const int W = 8 * sizeof(T);
    return getBit(x[i / W], i % W);
}

template <typename T> inline void setBitArr(T *x, uint i, bool b)
{
    const int W = 8 * sizeof(T);
    setBit(x[i / W], i % W, b);
}

inline bool blocksEqual(const block &x, const block &y)
{
    return cmpBlock(&x, &y, 1);
}

template <typename T> inline bool numericEquals(const T &x, const T &y)
{
    return x == y;
}

template <> inline bool numericEquals(const block &x, const block &y)
{
    return blocksEqual(x, y);
}

} // namespace emp
