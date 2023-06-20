#pragma once

#include "globals.h"
#include "rep_array_unsliced.h"
#include "bristol_fashion_array.h"

namespace emp {
namespace batcher {

template<typename T>
void swap_pairs(BristolFashion_array* compare_and_swap_circuit, rep_array_unsliced<T> arr, 
    uint64_t chunk_size, bool invert) {
    assert(__builtin_popcount(chunk_size) == 1 && "chunk_size power of 2");
    assert(chunk_size > 1);


    uint64_t even_len = (arr.length_Ts() / 2) * 2;

    rep_array_unsliced<T> circuit_input(even_len);
    rep_array_unsliced<T> circuit_output(even_len);

    // ceiling(arr.length_Ts() / chunk_size)
    for (uint64_t chunk_start = 0; chunk_start + chunk_size/2 < arr.length_Ts(); chunk_start += chunk_size) {
        uint64_t second_half_index_upper_bound = min(chunk_size/2, arr.length_Ts() - chunk_start - chunk_size/2);
        for (uint64_t i = 0; i < second_half_index_upper_bound; i++) {
            circuit_input.copy_Ts_from(arr, 1, chunk_start + chunk_size/2 + i,
                chunk_start + 2 * i + 1);
            uint64_t first_half_index;
            // interestingly, this almost worked with the if condition backwards
            if (invert) {
                first_half_index = chunk_start + chunk_size/2 - 1 - i;
            } else {
                first_half_index = chunk_start + i;
            }
            circuit_input.copy_Ts_from(arr, 1, first_half_index, chunk_start + 2 * i);
        }
    }

    compare_and_swap_circuit->compute_multithreaded(circuit_output, circuit_input, arr.length_Ts()/2);

    for (uint64_t chunk_start = 0; chunk_start + chunk_size/2 < arr.length_Ts(); chunk_start += chunk_size) {
        uint64_t second_half_index_upper_bound = min(chunk_size/2, arr.length_Ts() - chunk_start - chunk_size/2);
        for (uint64_t i = 0; i < second_half_index_upper_bound; i++) {
            arr.copy_Ts_from(circuit_output, 1, chunk_start + 2 * i + 1,
                chunk_start + chunk_size/2 + i);
            uint64_t first_half_index;
            if (invert) {
                first_half_index = chunk_start + chunk_size/2 - 1 - i;
            } else {
                first_half_index = chunk_start + i;
            }
            arr.copy_Ts_from(circuit_output, 1, chunk_start + 2 * i, first_half_index);
        }
    }

    circuit_input.destroy();
    circuit_output.destroy();
}

template<typename T>
void butterfly_head(BristolFashion_array* compare_and_swap_circuit, rep_array_unsliced<T> arr, uint64_t chunk_size) {
    swap_pairs(compare_and_swap_circuit, arr, chunk_size, true);
}

template<typename T>
void butterfly_body(BristolFashion_array* compare_and_swap_circuit, rep_array_unsliced<T> arr, uint64_t chunk_size) {
    if (chunk_size == 1) return;
    swap_pairs(compare_and_swap_circuit, arr, chunk_size, false);
    butterfly_body(compare_and_swap_circuit, arr, chunk_size/2);
}
 
template<typename T>
void butterfly(BristolFashion_array* compare_and_swap_circuit, rep_array_unsliced<T> arr, 
    uint64_t chunk_size) {
    if (chunk_size == 1) return;
    butterfly_head(compare_and_swap_circuit, arr, chunk_size);
    butterfly_body(compare_and_swap_circuit, arr, chunk_size / 2);
}

/*
Sort consecutive chunks of length chunk_size increasing, decreasing, increasing, decreasing
*/
template<typename T>
void sort_internal(BristolFashion_array* compare_and_swap_circuit, 
    rep_array_unsliced<T> arr, uint64_t chunk_size) {
    assert(__builtin_popcount(chunk_size) == 1 && "chunk_size power of 2");
    if (chunk_size == 1) {
        return;
    }
    sort_internal(compare_and_swap_circuit, arr, chunk_size/2);
    butterfly(compare_and_swap_circuit, arr, chunk_size);
}

/*
Doesn't work on 0
*/
uint64_t least_power_of_2_greater_than_or_equal_to(uint64_t len) {
    return 1ULL << (8 * sizeof(uint64_t) - __builtin_clzll(len - 1));
}

template<typename T>
void sort (BristolFashion_array* compare_and_swap_circuit, rep_array_unsliced<T> arr) {
    auto start = clock_start();
    sort_internal(compare_and_swap_circuit, arr, least_power_of_2_greater_than_or_equal_to(arr.length_Ts()));
    time_total_batcher += time_from(start);
}

}
}