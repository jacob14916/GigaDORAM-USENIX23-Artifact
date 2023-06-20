#pragma once


#include "debug.h"
#include "zero_sum_prg.h"
#include "rep_array_unsliced.h"
#include "rep_array_sliced.h"

namespace emp
{

class SHRepArray
{
public:

ZeroSumPRG zsprg;
RepNetIO* prev_io;
RepNetIO* next_io;

SHRepArray (uint resource_id)
:zsprg(resource_id),
prev_io(prev_ios[resource_id]),
next_io(next_ios[resource_id])
{

}

// I'm not using const exactly how it was suppoed to be used
template<typename S, int slice_sz_Ss>
    void start_mand_gate(const rep_array_sliced<S,slice_sz_Ss> &a, const rep_array_sliced<S,slice_sz_Ss> &b, rep_array_sliced<S,slice_sz_Ss> &c)
    {
        assert(a.length_slices == b.length_slices && b.length_slices == c.length_slices);
        int num_gates = a.length_slices;
        c.prg_write_next(zsprg);
        for (int i = 0; i < num_gates; i++)
        {
            // not sure if this is optimal way to write this (or if it matters at all), can measure
            c.xor_next_with_prev_AND_next(a,b,i);
            c.xor_next_with_prev_AND_next(b,a,i);
            c.xor_next_with_prev_AND_prev(a,b,i);
        }
        c.io_send_next(next_io);
    }

template<typename S, int slice_sz_Ss>
    void finish_mand_gate(const rep_array_sliced<S,slice_sz_Ss> &a, const rep_array_sliced<S,slice_sz_Ss> &b, rep_array_sliced<S,slice_sz_Ss> &c) 
    {
        c.io_recv_prev(prev_io);
    }
    

template<typename S, int slice_sz_Ss>
    void mand_gate(const rep_array_sliced<S,slice_sz_Ss> &a, const rep_array_sliced<S,slice_sz_Ss> &b, rep_array_sliced<S,slice_sz_Ss> &c) 
    {
        for(int i = 0; i<((a.length_slices-1)>>16) + 1; i++){
            int window_size = min(a.length_slices - (i<<16) , 1<<16);
            rep_array_sliced<S, slice_sz_Ss> a_window = a.window_sliced(window_size, i<<16);
            rep_array_sliced<S, slice_sz_Ss> b_window = b.window_sliced(window_size, i<<16);
            rep_array_sliced<S, slice_sz_Ss> c_window = c.window_sliced(window_size, i<<16);
            start_mand_gate(a_window, 
                            b_window, 
                            c_window);
            next_io->flush();
            finish_mand_gate(a_window, 
                            b_window, 
                            c_window);
        }
    }

// who uses this?
    template<typename T> 
    void if_then_else(rep_array_unsliced<int> condition, rep_array_unsliced<T> then_result, 
    rep_array_unsliced<T> else_result, rep_array_unsliced<T> output) {
        assert(then_result.length_Ts() == 1);
        assert(else_result.length_Ts() == 1);
        assert(output.length_Ts() == 1);

        // these share memory with the non-sliced versions
        rep_array_sliced<T, 1> then_result_sliced(then_result);
        rep_array_sliced<T, 1> else_result_sliced(else_result);
        rep_array_sliced<T, 1> output_sliced(output);

        rep_array_sliced<T, 1> condition_extended(1);
        condition_extended.extend(condition);
        rep_array_sliced<T, 1> then_xor_else(1);
        then_xor_else.copy(then_result_sliced);
        then_xor_else.xor_with(else_result_sliced);
        mand_gate(condition_extended, then_xor_else, output_sliced);
        output_sliced.xor_with(else_result_sliced);
        condition_extended.destroy();
        then_xor_else.destroy();
    }

};

} // namespace emp

// SupportsXOR _del_temp_clear;
// SupportsXOR _del_temp_clear_before;
// bool _del_temp_clear_equals;

// reveal_to(1, &ls[i], &_del_temp_clear_before, 1);

// reveal_to(1, &ls[i], &_del_temp_clear, 1);
// reveal_to(1, &equals_null, &_del_temp_clear_equals, 1);
//
// if (party == 1)
//{
//    dbg("in relabel dummies ", i, " in clear ", _del_temp_clear, " and before ", _del_temp_clear_before,
//        " where equals_null ", _del_temp_clear_equals);
//    assert(_del_temp_clear < (uint)(1 << (log_adress_space + 1)));
//}