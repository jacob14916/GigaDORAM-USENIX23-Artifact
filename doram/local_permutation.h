#pragma once

#include "emp-tool/emp-tool.h"
#include "utils.h"

namespace emp
{

class LocalPermutation
{
  public:
    uint n;
    uint *fy;
    uint *pi = nullptr;

    LocalPermutation(const LocalPermutation &rhs) {
        *this = rhs;
    }

    LocalPermutation &operator=(const LocalPermutation &rhs) {
        n = rhs.n;
        fy = new uint[n];
        memcpy(fy, rhs.fy, n * sizeof(uint));
        return *this;
    }

    LocalPermutation(PRG *prg, uint n) : n(n)
    {
        assert(prg != nullptr && n > 0);
        // TODO: convert this to static allocation
        fy = new uint[n];
        fy[0] = 0;
        for (uint i = 1; i < n; i++)
        {
            fy[i] = sample_unif_from_prg(prg, 0, i + 1);
        }
    }

    ~LocalPermutation()
    {
        delete[] fy;
        if (pi != nullptr)
            delete[] pi;
    }

    //! both shuffle functions would be better with std::swap, but I have been getting pages of warnings, so I'll do it
    //! manually. I think it was complaining about the fact the arguments were const
    template <typename T> void shuffle(T *arr)
    {
        T tmp;
        for (uint i = n - 1; i != 0; i--)
        {
            // swap i and fy[i]
            // std::swap(arr[i], arr[fy[i]]);
            tmp = arr[i];
            arr[i] = arr[fy[i]];
            arr[fy[i]] = tmp;
        }
    }

    // working shuffle of bits,
    // not necessarily fastest 
    // (may have to use processor-specific insns
    // but this code can certainly be further optimized 
    // even without doing so)
    void bit_shuffle(uint8_t* arr)
    {
        uint8_t tmp_i, tmp_fy, chg;
        for(uint i = n-1; i!=0; i--)
        {
            tmp_i = ( (arr[i/8]>>(7-(i%8))) & 1 );
            tmp_fy = ( (arr[fy[i]/8] >>(7-(fy[i]%8))) & 1 );
            chg = tmp_i ^ tmp_fy;
            arr[i/8] ^= chg<<(7-(i%8));
            arr[fy[i]/8] ^= chg<<(7-(fy[i]%8));
        }
    }

    template <typename T> void inverse_shuffle(T *arr)
    {
        T tmp;
        for (uint i = 1; i < n; i++)
        {
            // swap i and fy[i]
            // std::swap(arr[i], arr[fy[i]]);
            tmp = arr[i];
            arr[i] = arr[fy[i]];
            arr[fy[i]] = tmp;
        }
    }

    uint evaluate_at(uint input)
    {
        assert(input < n);
        if (pi == nullptr) // then make the non fisher-yates version of the array
        {
            // we have that shuffle(arr)[pi[i]] = arr[i]
            pi = new uint[n];
            for (uint i = 0; i < n; i++)
            {
                pi[i] = i;
            }
            inverse_shuffle(pi);
        }
        return pi[input];
    }
};
} // namespace emp