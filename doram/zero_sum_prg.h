#pragma once

#include "block.h"
#include "emp-tool/emp-tool.h"
#include "globals.h"

namespace emp
{

// conveniently acccomodates 1024 = 8 * 128 of the largest levels in AES (180) 
const int ZSPRG_BUF_SZ_BLOCKS = 8 * 180;

class ZeroSumPRG
{
  public:
    // optimization: larger buffer
    block* buffer_prev;
    block* buffer_next;
    uint64_t counter = 0;
    PRG* prev_prg;
    PRG* next_prg;

    explicit ZeroSumPRG(uint resource_id)//thread_id 
    :prev_prg(prev_prgs[resource_id]),
    next_prg(next_prgs[resource_id])
    {
        buffer_prev = new block[ZSPRG_BUF_SZ_BLOCKS];
        buffer_next = new block[ZSPRG_BUF_SZ_BLOCKS];
    }

    ~ZeroSumPRG()
    {
        delete[] buffer_prev;
        delete[] buffer_next;
    }

    void random_block(block *data, int nblocks) {
        while (nblocks > 0) {
            int blocks_this_iteration = min(ZSPRG_BUF_SZ_BLOCKS, nblocks);
            prev_prg->random_block(buffer_prev, blocks_this_iteration);
            next_prg->random_block(buffer_next, blocks_this_iteration);
            for (int i = 0; i < blocks_this_iteration; i++) {
                data[i] = buffer_prev[i] ^ buffer_next[i];
            }
            nblocks -= blocks_this_iteration;
        }
    }

    // functions copied straight from emp PRG.h
    void random_data(void *data, int nbytes) {
		random_block((block *)data, nbytes/16);
		if (nbytes % 16 != 0) {
			block extra;
			random_block(&extra, 1);
			memcpy((nbytes/16*16)+(char *) data, &extra, nbytes%16);
		}
	}

	void random_bool(bool * data, int length) {
		uint8_t * uint_data = (uint8_t*)data;
		random_data_unaligned(uint_data, length);
		for(int i = 0; i < length; ++i)
			data[i] = uint_data[i] & 1;
	}

	void random_data_unaligned(void *data, int nbytes) {
		size_t size = nbytes;
		void *aligned_data = data;
		if(std::align(sizeof(block), sizeof(block), aligned_data, size)) {
			int chopped = nbytes - size;
			random_data(aligned_data, nbytes - chopped);
			block tmp[1];
			random_block(tmp, 1);
			memcpy(data, tmp, chopped);
		} else {
			block tmp[2];
			random_block(tmp, 2);
			memcpy(data, tmp, nbytes);
		}
	}

    // !next_bit() is deprecated
    bool next_bit()
    {
        bool result;
        random_bool(&result, 1);
        return result;
    }
};
} // namespace emp