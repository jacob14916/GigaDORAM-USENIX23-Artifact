#pragma once

#include "debug.h"
#include "local_permutation.h"
#include "rep_array_unsliced.h"
#include "zero_sum_prg.h"

namespace emp
{
/*
Input: 16 8-byte rows
Output: 8 16-byte columns

As explained in the paper, the bit transpose of a 16 x 64 bit rect happens in stages.
First we do a byte transpose, by serially applying unpacklo and unpackhi.
This leaves us with byte-columns stored in __m128i registers. 
Next we extract bit-columns one at a time using movemask.
*/


template<typename S, int slice_sz_Ss>
class rep_array_sliced {
public:
    int length_slices;
    bool is_original = false;
private:
    S* prev;
    S* next;
public:
    explicit rep_array_sliced(int length_slices)
    :length_slices(length_slices), is_original(true)
    {
        int length_Ss = total_length_Ss();
        prev = new S[length_Ss];
        next = new S[length_Ss]; 
        memset(prev, 0, length_Ss * sizeof(S));
        memset(next, 0, length_Ss * sizeof(S));
    }

    template<typename T>
    explicit rep_array_sliced(rep_array_unsliced<T>& unsliced, const int total_slices_override = -1)
    : is_original(true) {
        // set length_slices to override or implied by input if no override
        assert(unsliced.length_bytes % (sizeof(S) * slice_sz_Ss) == 0);
        int input_length_slices = unsliced.length_bytes / sizeof(S) / slice_sz_Ss; 
        if (total_slices_override == -1) {
            length_slices = input_length_slices;
        } else {
            assert(total_slices_override >= input_length_slices);
            length_slices = total_slices_override;
        }

        // allocate prev and next
        int length_Ss = total_length_Ss();
        prev = new S[length_Ss];
        next = new S[length_Ss]; 
        memset(prev, 0, length_Ss * sizeof(S));
        memset(next, 0, length_Ss * sizeof(S));

        // transpose input in
        auto start = clock_start();
        sse_trans((uint8_t*)prev, (uint8_t*)(unsliced.prev), slice_sz_bits(), input_length_slices);
        sse_trans((uint8_t*)next, (uint8_t*)(unsliced.next), slice_sz_bits(), input_length_slices);
        time_total_transpose += time_from(start);

        // todo: patch in bit_transpose_block for larger sizes
    }

    // caution: cast-like same memory "constructor". use only if you know what you're doing
    explicit rep_array_sliced(rep_array_unsliced<S> unsliced) 
    :length_slices(unsliced.length_Ts())
    {
        // probably a way to make this constructor only work for slice_sz_Ss == 1, but idk
        assert(slice_sz_Ss == 1);
        prev = unsliced.prev;
        next = unsliced.next;
    }

    template<typename T>
    void unslice(rep_array_unsliced<T>& unsliced) {
        assert(unsliced.length_bytes == total_length_Ss() * sizeof(S));
        auto start = clock_start();
        sse_trans((uint8_t*)(unsliced.prev), (uint8_t*)prev, length_slices, slice_sz_bits());
        sse_trans((uint8_t*)(unsliced.next), (uint8_t*)next, length_slices, slice_sz_bits());
        time_total_transpose += time_from(start);

        // todo: patch in bit_transpose_block for larger sizes
    }

    void destroy() {
        assert(is_original);
        delete[] prev;
        delete[] next;
    }

    inline int slice_sz_bits () {
        return slice_sz_Ss * sizeof(S) * 8;
    }

    inline int total_length_Ss () {
        return length_slices * slice_sz_Ss;
    }

    inline int total_length_bytes () {
        return total_length_Ss() * sizeof(S);
    }

inline rep_array_sliced<S, slice_sz_Ss> window_sliced(int num_slices, int offset_slices)const {
    assert(num_slices + offset_slices <= length_slices);
    // using default copy constructor
    rep_array_sliced<S, slice_sz_Ss> window = *this;
    window.prev += offset_slices * slice_sz_Ss;
    window.next += offset_slices * slice_sz_Ss;
    window.length_slices = num_slices;
    window.is_original = false;
    return window;
}

inline void repcpy_sliced(rep_array_sliced<S, slice_sz_Ss>& dst,  int num_slices, 
                    int src_offset_slices = 0, int dst_offset_slices = 0) 
{
    assert(src_offset_slices + num_slices <= length_slices);
    assert(dst_offset_slices + num_slices <= dst.length_slices);
    int num_bytes = num_slices * slice_sz_Ss * sizeof(S);
    // scary to give raw access to prev 
    memcpy(dst.prev + slice_sz_Ss * dst_offset_slices, prev + slice_sz_Ss * src_offset_slices, num_bytes);
    memcpy(dst.next + slice_sz_Ss * dst_offset_slices, next + slice_sz_Ss * src_offset_slices, num_bytes);
}

inline void xor_indices(int src1_slices, int src2_slices, int dst_slices) {
    for (int j = 0; j < slice_sz_Ss; j++) {
        prev[dst_slices * slice_sz_Ss + j] = prev[src1_slices * slice_sz_Ss + j] ^ prev[src2_slices * slice_sz_Ss + j];
    }
    for (int j = 0; j < slice_sz_Ss; j++) {
        next[dst_slices * slice_sz_Ss + j] = next[src1_slices * slice_sz_Ss + j] ^ next[src2_slices * slice_sz_Ss + j];
    }
}

inline void not_indices(int src_slices, int dst_slices) {
    S all_one = get_all_ones_of_type<S>();
    for (int j = 0; j < slice_sz_Ss; j++) {
        prev[dst_slices * slice_sz_Ss + j] = prev[src_slices * slice_sz_Ss + j] ^ all_one; 
        next[dst_slices * slice_sz_Ss + j] = next[src_slices * slice_sz_Ss + j] ^ all_one; 
    }
}

inline void copy_indices(int src_slices, int dst_slices) {
    repcpy_sliced(*this, 1, src_slices, dst_slices);
}

inline void xor_next_with_prev_AND_prev(const rep_array_sliced<S, slice_sz_Ss>& a, const rep_array_sliced<S, slice_sz_Ss>& b, int slice_index) {
    int slice_start_Ss = slice_index * slice_sz_Ss;
    for (int j = 0; j < slice_sz_Ss; j++) {
        int i = slice_start_Ss + j;
        next[i] ^= a.prev[i] & b.prev[i];
    }
}

inline void xor_next_with_prev_AND_next(const rep_array_sliced<S, slice_sz_Ss>& a, const rep_array_sliced<S, slice_sz_Ss>& b, int slice_index) {
    int slice_start_Ss = slice_index * slice_sz_Ss;
    for (int j = 0; j < slice_sz_Ss; j++) {
        int i = slice_start_Ss + j;
        next[i] ^= a.prev[i] & b.next[i];
    }
}


void xor_shares_into_block(S* dst) const {
    for (int i = 0; i < total_length_Ss(); i++){
        dst[i] ^= prev[i] ^ next[i];
    }
}

// add the shares from a into this rep array
inline void xor_shares(const rep_array_sliced<S, slice_sz_Ss>& a){
    assert(a.total_length_Ss() == total_length_Ss());
    for(int i = 0; i < total_length_Ss(); i++){
        prev[i] ^= a.prev[i];
        next[i] ^= a.next[i];
    }
}

// add the block b to both prev and next
inline void xor_block_into_shares(S* b){
    for(int i = 0; i < total_length_Ss(); i++){
        prev[i] ^= b[i];
        next[i] ^= b[i];
    }
}

// multiply both prev and next, entry-wise, by the scalar block
inline void scalar_mult(S* scalar){
    for(int i = 0; i < total_length_Ss(); i++){
        prev[i] &= scalar[i];
        next[i] &= scalar[i];
    }
}

// set this block as the xor of a and b
inline void set_as_xor(const rep_array_sliced& a, const rep_array_sliced& b){
    assert(a.total_length_Ss() == total_length_Ss() &&
            b.total_length_Ss() == total_length_Ss());
    for (int i = 0; i < total_length_Ss(); i++){
        prev[i] = a.prev[i] ^ b.prev[i];
        next[i] = a.next[i] ^ b.next[i];
    }
}

// shuffle block-wise
void shuffle(LocalPermutation* shuffler){
    shuffler->shuffle(next);
    shuffler->shuffle(prev);
}

void extend(rep_array_unsliced<int> bit) {
    assert(slice_sz_Ss == 1);
    assert(length_slices == 1);
    S select_mask[2];
    select_mask[0] = get_all_zeros_of_type<S>();
    select_mask[1] = get_all_ones_of_type<S>();
    *prev = select_mask[bit.prev[0] & 1];
    *next = select_mask[bit.next[0] & 1];
}

void xor_with(rep_array_sliced<S, slice_sz_Ss> other) {
    assert(other.total_length_Ss() == total_length_Ss());
    for (int i = 0; i < total_length_Ss(); i++) {
        prev[i] ^= other.prev[i];
        next[i] ^= other.next[i];
    }
}

void copy(rep_array_sliced<S, slice_sz_Ss> other) {
    assert(other.total_length_Ss() == total_length_Ss());
    memcpy(prev, other.prev, total_length_bytes());
    memcpy(next, other.next, total_length_bytes());
}

template<typename PRG_t>
void fill_random (PRG_t* prev_prg, PRG_t* next_prg) {
    prev_prg->random_data(prev, total_length_bytes());
    next_prg->random_data(next, total_length_bytes());
}

// I'd maybe like to use function pointers here, but this works
template<typename PRG_t> void prg_write_next(PRG_t& prg) {
    prg.random_data(next, total_length_bytes());
}

template<typename PRG_t> void prg_write_prev(PRG_t& prg) {
    prg.random_block(prev, total_length_Ss());
}

template<typename IO_t>
void io_send_next(IO_t* next_io) {
    next_io->send_data(next, total_length_bytes());
}

template<typename IO_t>
void io_recv_prev(IO_t* prev_io) {
    prev_io->recv_data(prev, total_length_bytes());
}

void debug_print_slice(string desc, uint index_to_print) {
    using namespace thread_unsafe;
    vector<S> clear(slice_sz_Ss);
    next_io->send_data(&prev[index_to_print * slice_sz_Ss], slice_sz_Ss * sizeof(S));
    prev_io->recv_data(clear.data(), slice_sz_Ss * sizeof(S));
    dbg(desc, clear);
}

// send prev share (used to open a value)
template<typename IO_t>
void io_send_prev_to_next(IO_t* next_io) const {
    next_io->send_block(prev, total_length_Ss());
}

// for when parties are passing in a circle (to next, from prev)
// controlled send-and-receive, to avoid deadlock
// lengths should be the same because this will be used 
// when all players are sending and receiving shares of the same information
// TODO: find a better place for this
template<typename IO_t>
static void inline io_send_and_receive(IO_t * prev_io, IO_t * next_io, block* send, block* recv, int length)
{
    for(int i = 0; i<((length-1)>>16) + 1; i++){
        int window_size = min(length - (i<<16) , 1<<16);
        next_io->send_block(send, window_size);
        next_io->flush();
        prev_io->recv_block(recv, window_size);
        send += window_size;
        recv += window_size;
    }
}

// most common version
void inline io_send_next_and_recv_prev() const 
{
    io_send_and_receive(next, prev, total_length_Ss());
}

void inline io_send_and_recv_next(block* recv) const
{
    io_send_and_receive(next, recv, total_length_Ss());
}

void inline io_send_and_recv_prev(block* recv) const
{
    io_send_and_receive(prev, recv, total_length_Ss());
}

};


}