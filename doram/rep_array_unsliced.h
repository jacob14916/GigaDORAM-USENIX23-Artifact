#pragma once

#include "debug.h"
#include "zero_sum_prg.h"
#include "local_permutation.h"

using namespace std;


namespace emp
{


template<typename T>
class rep_array_unsliced {

    template<typename S> friend class rep_array_unsliced;
    template<typename S, int slice_sz_Ss> friend class rep_array_sliced;
    static_assert(!std::is_same<bool, T>::value, "xor doesn't work with bools that aren't 0 or 1");
private:
    T* prev;
    T* next;
public:
    uint64_t length_bytes;
    inline uint64_t length_Ts() {return length_bytes / sizeof(T);}

    // rep_array_unsliced is zero initialized
    explicit rep_array_unsliced(uint64_t length_Ts)
    :length_bytes(length_Ts * sizeof(T)) {
        assert(length_Ts > 0);
        prev = new T[length_Ts];
        next = new T[length_Ts];
        memset(prev, 0, length_bytes);
        memset(next, 0, length_bytes);
    }

    void destroy () {
        delete[] prev;
        delete[] next;
    }

private:
    explicit rep_array_unsliced(T* prev, T* next, uint64_t length_Ts) 
    :prev(prev), next(next), length_bytes(length_Ts * sizeof(T))
    {
    }


public:

void input(int party_inputting, const T *secrets) 
{
    using namespace thread_unsafe;
    if (party == party_inputting) {
        prev_prg->random_data(prev, length_bytes);
        for (uint i = 0; i < length_Ts(); i++) {
            next[i] = secrets[i] ^ prev[i];
        }

        next_io->send_data(next, length_bytes);
    } 
    else if (party == prev_party(party_inputting)) {
        //! before this memset() call was added
        //! there was a bug when reusing a rep_array_unsliced by calling input()
        memset(prev, 0, length_bytes);
        next_prg->random_data(next, length_bytes);
    } else {
        prev_io->recv_data(prev, length_bytes);
        memset(next, 0, length_bytes);
    }
}

void input_xor(int party_inputting, const T *secrets) {
    using namespace thread_unsafe;
    // this is just a subroutine of reshare 2 to 3
    T* pad = new T[length_Ts()];
    if (party == party_inputting) {
        prev_prg->random_data(pad, length_bytes);
        for (uint i = 0; i < length_Ts(); i++) {
            prev[i] ^= pad[i];
            pad[i] ^= secrets[i];
            next[i] ^= pad[i];
        }
        next_io->send_data(pad, length_bytes);
    } else if (party == prev_party(party_inputting)) {
        next_prg->random_data(pad, length_bytes);
        for (uint i = 0; i < length_Ts(); i++) {
            next[i] ^= pad[i];
        }
    } else {
        prev_io->recv_data(pad, length_bytes);
        for (uint i = 0; i < length_Ts(); i++) {
            prev[i] ^= pad[i];
        }
    }
    delete[] pad;
}

void input_public(const T* public_data) {
    if (party == 1) {
        memcpy(next, public_data, length_bytes);
    } else if (party == 2) {
        memcpy(prev, public_data, length_bytes);
    } else {
    }
}

void reveal_to(int party_receiving, T *secrets) {
    using namespace thread_unsafe;
    if (party == party_receiving) {
        prev_io->recv_data(secrets, length_bytes);
        for (uint i = 0; i < length_Ts(); i++) {
            secrets[i] ^= (prev[i] ^ next[i]);
        }
    } else if (party == prev_party(party_receiving)) {
        next_io->send_data(prev, length_bytes);
    } 
}

void reveal_to_all(T* public_values) {
    io_send_and_recv(prev, public_values, length_bytes);
    // next_io->send_data(prev, length_bytes);
    // next_io->flush();
    // prev_io->recv_data(public_values, length_bytes);
    for (uint i = 0; i < length_Ts(); i++) {
        public_values[i] ^= (prev[i] ^ next[i]);
    }
}

void from_2shares(int from_1, int from_2, const T* reg_shares) {
    // this doesn't have to be 2 rounds
    // but wait, is it?f
    // it's two rounds if the same party first waits to receive and then send
    // to make it not two rounds, we just need from_2 = prev_party(from_1)
    // which is maybe counterintuitive, consider switching
    if(from_2 == prev_party(from_1)){
        input(from_1, reg_shares);
        input_xor(from_2, reg_shares);
    }
    else{
        input(from_2, reg_shares);
        input_xor(from_1, reg_shares);
    }
    
}

void reshare_3to2(int to_1, int to_2, T* reg_shares) {
    assert(to_1 != to_2);
    if (party == to_1) {
        for (uint i = 0; i < length_Ts(); i++) {
            reg_shares[i] = prev[i] ^ next[i];
        }
    } else if (party == to_2) {
        // missing share is prev(prev_party(to_1)) = next(next_party(to_1))
        if (to_2 == prev_party(to_1)) {
            for (uint i = 0; i < length_Ts(); i++) {
                reg_shares[i] = prev[i];
            }
        } else {
            for (uint i = 0; i < length_Ts(); i++) {
                reg_shares[i] = next[i];
            }
        }
    }
}

void inline reshare_2to2(T* reg_shares, int from_1, int from_2, int to_1, int to_2)
{
    from_2shares(from_1, from_2, reg_shares);
    reshare_3to2(to_1, to_2, reg_shares);
}

template<typename S> void copy(rep_array_unsliced<S> &src) {
    assert(src.length_bytes == length_bytes);
    memcpy(prev, src.prev, length_bytes);
    memcpy(next, src.next, length_bytes);
}


template<typename S> void copy_one(uint dst_index, rep_array_unsliced<S>& src, uint src_index = 0) {
    assert(src_index < src.length_bytes/sizeof(S));
    assert(dst_index < length_Ts());
    prev[dst_index] = (src.prev)[src_index];
    next[dst_index] = (src.next)[src_index];
}

template<typename S> void copy_bytes_from(rep_array_unsliced<S>& src, uint64_t num_bytes, 
                    uint64_t src_offset_bytes = 0, uint64_t dst_offset_bytes = 0) 
{
    assert(src_offset_bytes + num_bytes <= src.length_bytes);
    assert(dst_offset_bytes + num_bytes <= length_bytes);
    memcpy(((uint8_t*)prev) + dst_offset_bytes, ((uint8_t*)src.prev) + src_offset_bytes, num_bytes);
    memcpy(((uint8_t*)next) + dst_offset_bytes, ((uint8_t*)src.next) + src_offset_bytes, num_bytes);
}

void random_data_thread_unsafe()
{
    using namespace thread_unsafe;
    prev_prg->random_block(prev, length_Ts());
    next_prg->random_block(next, length_Ts());
}

template<typename PRG_t>
void random_data(PRG_t* prev_prg, PRG_t* next_prg) {
    prev_prg->random_data(prev, length_bytes);
    next_prg->random_data(next, length_bytes);
}


inline rep_array_unsliced<T> window_sliced(uint64_t num_slices, uint64_t offset_slices) {
    assert(num_slices + offset_slices <= length_Ts());
    // using default copy constructor
    rep_array_unsliced<T> window = *this;
    window.next += offset_slices;
    window.prev += offset_slices;
    window.resize(num_slices);
    return window;
}

void f();

// functions needed:
// multiply
// add
inline void add_next_to_prev_times_next(rep_array_unsliced<block>& a,
                                        rep_array_unsliced<block>& b);

inline void add_next_to_prev_times_prev(rep_array_unsliced<block>& a,
                                        rep_array_unsliced<block>& b);

template<typename IO_t>
inline void io_send_next(IO_t * next_io);

template<typename IO_t>
inline void io_recv_prev(IO_t * prev_io);

static void inline io_send_and_recv(T* send, T* recv, uint64_t length_bytes)
{
    using namespace thread_unsafe;
    for(uint64_t i = 0; i != ((length_bytes-1)>>20)+1; i++){
        uint64_t window_size = min(length_bytes - (i<<20), 1UL<<20);
        next_io->send_data(send, window_size);
        next_io->flush();
        prev_io->recv_data(recv, window_size);
        send += window_size;
        recv += window_size;
    }
}

void inline io_send_next_and_recv_prev()
{
    io_send_and_recv(next,prev,length_bytes);
}

inline void xor_indices(int src1_index, int src2_index, int dst_index);


void copy_Ts_from(rep_array_unsliced<T> &src, uint num_Ts, uint src_offset_Ts = 0, uint dst_offset_Ts = 0) {
    copy_bytes_from(src, num_Ts * sizeof(T), src_offset_Ts * sizeof(T), dst_offset_Ts * sizeof(T));
}

void shuffle_by(LocalPermutation* perm) {
    assert(length_Ts() == perm->n);
    perm->shuffle(prev);
    perm->shuffle(next);
}

void reverse() {
    uint half_length = length_Ts() / 2;
    for (uint i = 0; i < half_length; i++) {
        std::swap(prev[i], prev[length_Ts() - 1 - i]);
        std::swap(next[i], next[length_Ts() - 1 - i]);
    }
}

vector<T> to_fixed() {
    vector<T> fixed(length_Ts() * 3);
    int prev_share_loc = party - 1;
    int next_share_loc = party % 3;
    memcpy(fixed.data() + prev_share_loc * length_Ts(), prev, length_bytes);
    memcpy(fixed.data() + next_share_loc * length_Ts(), next, length_bytes);
    return fixed;
}

void of_fixed(vector<T> &fixed) {
    assert(fixed.size() == length_Ts() * 3);
    int prev_share_loc = party - 1;
    int next_share_loc = party % 3;
    memcpy(prev, fixed.data() + prev_share_loc * length_Ts(), length_bytes);
    memcpy(next, fixed.data() + next_share_loc * length_Ts(), length_bytes);
}

void debug_print (string comment, uint at_most = UINT_MAX) {
    uint num_to_reveal = length_Ts();
    if (at_most != UINT_MAX && at_most < length_Ts()) {
        num_to_reveal = at_most;
    }
    vector<T> clear(num_to_reveal);
    window(0, num_to_reveal).reveal_to_all(clear.data());
    dbg(comment, clear);
}

void test_print (uint at_most = UINT_MAX) {
    vector<T> clear(length_Ts());
    reveal_to_all(clear.data());
    if (at_most != UINT_MAX && length_Ts() > at_most) {
        clear.resize(at_most);
    }
    string sep = "";
    for (T t : clear) {
        cout << sep << t;
        sep = " ";
    }
    cout << '\n';
}

void test_if_else_on_sizeof_t () {
    if (sizeof(T) == 16) {
        cout << "block\n";
    } else if (sizeof(T) == 8) {
        cout << "long long\n";
    } else if (sizeof(T) <= 4) {
        cout << "int or shorter\n";
    } else {
        cout << "something else\n";
    }
}

void fill_random() {
    using namespace thread_unsafe;
    prev_prg->random_data(prev, length_bytes);
    next_prg->random_data(next, length_bytes);
}


void xor_with(rep_array_unsliced<T> &other) {
    assert(length_bytes == other.length_bytes);
    for (uint i = 0; i < length_Ts(); i++) {
        prev[i] ^= other.prev[i];
        next[i] ^= other.next[i];
    }
}

rep_array_unsliced<T> xor_of_all_elements () {
    rep_array_unsliced<T> result(1); 
    for (uint i = 0; i < length_Ts(); i++) {
        result.prev[0] ^= prev[i];
        result.next[0] ^= next[i];
    }
    return result;
}


void apply_or_mask (T mask) {
    for (uint i = 0; i < length_Ts(); i++) {
        prev[i] = prev[i] | mask;
        next[i] = next[i] | mask;
    }
}

void apply_and_mask (T mask) {
    for (uint i = 0; i < length_Ts(); i++) {
        prev[i] = prev[i] & mask;
        next[i] = next[i] & mask;
    }
}

void extract_bit_xor (uint index, rep_array_unsliced<int> bit) {
    assert(bit.length_Ts() == 1);
    bit.prev[0] ^= getBitArr(prev, index);
    bit.next[0] ^= getBitArr(next, index);
}

rep_array_unsliced<T> window(uint start, uint length_Ts) {
    return rep_array_unsliced(prev + start, next + start, length_Ts);
}


};

template<>
template<typename S>
void rep_array_unsliced<block>::copy_one(uint dst_index, rep_array_unsliced<S>& src, uint src_index) {
    assert(src_index < src.length_bytes/sizeof(S));
    assert(dst_index < length_Ts());
    prev[dst_index] = makeBlock(0, (src.prev)[src_index]);
    next[dst_index] = makeBlock(0, (src.next)[src_index]);
}

template<>
template<>
void rep_array_unsliced<block>::copy_one(uint dst_index, rep_array_unsliced<block>& src, uint src_index) {
    assert(src_index < src.length_Ts());
    assert(dst_index < length_Ts());
    prev[dst_index] = (src.prev)[src_index];
    next[dst_index] = (src.next)[src_index];
}

template<>
void rep_array_unsliced<block>::f()
{
    return;
}

template<>
template<typename IO_t>
inline void rep_array_unsliced<block>::io_send_next(IO_t * next_io){
    next_io->send_block(next, length_Ts());
}

template<>
template<typename IO_t>
inline void rep_array_unsliced<block>::io_recv_prev(IO_t * prev_io){
    prev_io->recv_block(prev, length_Ts());
}


}

