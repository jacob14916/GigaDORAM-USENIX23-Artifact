#pragma once
#include <emp-tool/emp-tool.h>
#include "debug.h"
#include "sh_riro_garble.h"
#include <vector>
#include <stack>


using namespace std;

namespace emp {

namespace optimalcht {
using namespace thread_unsafe;

// Set me in main!
sh_riro::Circuit* lookup_circuit;

double time_in_circuit, time_before_circuit, time_after_circuit;
// some special values that fit into a uint
// as signed ints, these are -1, -2, -3, etc
const uint NONE = -1;
const uint ROOT = -2;
const uint UNVISITED = -3;
const uint STASHED = -4;

struct directed_edge {
        uint edge;
        uint vertex;
    };


inline uint h0(const block* b, uint log_single_col_len) {
    const uint hash_mask = (1 << log_single_col_len) - 1;
    ull hi = ((ull*)b)[1]; 
    return hi & hash_mask;
}

inline uint h1(const block* b, uint log_single_col_len) {
    const uint hash_mask = (1 << log_single_col_len) - 1;
    ull hi = ((ull*)b)[1]; 
    return ((hi >> 32) & hash_mask) | (1 << log_single_col_len);
}

void build(vector<block> &table, uint log_single_col_len, const vector<block> &input_array, vector<uint> &stash_indices)
{
    table.resize(2 << log_single_col_len);
    dbg("Starting CHT build");
    uint num_edges = input_array.size();
    dbg_args(num_edges)
    uint num_vertices = 2 << log_single_col_len;

    // algorithm: DFS through the graph of locations with edges formed by elements
    vector<vector<directed_edge>> edges(num_vertices);
    for (uint edge = 0; edge < num_edges; edge++) {
        uint left_vertex = h0(&input_array[edge], log_single_col_len);
        uint right_vertex = h1(&input_array[edge], log_single_col_len);
        edges[left_vertex].push_back({edge, right_vertex});
        edges[right_vertex].push_back({edge, left_vertex});
    }

    vector<uint> state(num_edges, UNVISITED);
    vector<uint> parent_vertex(num_vertices, NONE);
    vector<uint> parent_edge(num_vertices, NONE);
    stack<uint> dfs;
    for (uint starting_vertex = 0; starting_vertex < num_vertices; starting_vertex++) {
        if (parent_vertex[starting_vertex] != NONE) continue;
        uint extra_edge = NONE;
        // build DFS tree 
        // it's easier to think about vertices = locations
        // danger of type error
        vector<uint> component;
        parent_vertex[starting_vertex] = ROOT;
        parent_edge[starting_vertex] = ROOT;
        dfs.push(starting_vertex);
        while (!dfs.empty()) {
            uint curr_vertex = dfs.top(); dfs.pop();
            component.push_back(curr_vertex);
            for (directed_edge& d : edges[curr_vertex]) {
                if (state[d.edge] == UNVISITED) {
                    if (parent_vertex[d.vertex] == NONE)
                    {
                        parent_vertex[d.vertex] = curr_vertex;
                        parent_edge[d.vertex] = d.edge;
                        state[d.edge] = d.vertex;
                        dfs.push(d.vertex);
                    } else if (extra_edge == NONE) {
                        extra_edge = d.edge;
                        state[d.edge] = d.vertex;
                    } else {
                        state[d.edge] = STASHED;
                    }
                } 
            }
        }

        for (uint c : component) {
            (void)c;
            assert(parent_edge[c] != NONE);
        }

        if (extra_edge != NONE) {
            uint vertex_to_reorient_away_from = state[extra_edge];
            while (parent_vertex[vertex_to_reorient_away_from] != ROOT) {
                state[parent_edge[vertex_to_reorient_away_from]] = parent_vertex[vertex_to_reorient_away_from];
                vertex_to_reorient_away_from = parent_vertex[vertex_to_reorient_away_from];
            }
        }
    }

    uint num_marked_stashed = 0;
    for (uint edge = 0; edge < num_edges; edge++) {
        num_marked_stashed += (state[edge] == STASHED);
    }

    uint stash_length = stash_indices.size();
    uint stash_deficit = stash_length - num_marked_stashed;
    dbg_args(num_marked_stashed, stash_length, stash_deficit);
    assert(num_marked_stashed <= stash_length);

    uint num_stashed = 0;
    for (uint edge = 0; edge < num_edges; edge++) {
        if (state[edge] == STASHED) 
        {
            // here we depend on the blocks of input_array having their (builder-order) indices appended
            // to take up the entire 4 lowest bytes
            stash_indices[num_stashed++] = ((uint*)(&input_array.data()[edge]))[0];
        } 
        else 
        {
            assert(state[edge] != UNVISITED);
            if (stash_deficit > 0) {
                stash_indices[num_stashed++] = ((uint*)(&input_array.data()[edge]))[0];
                stash_deficit--;
            } else {
                table[state[edge]] = input_array[edge];
            }
        }
    }
    assert(stash_deficit == 0);
}

uint lookup_from_2shares (block* table_2share, block key, uint log_single_col_len, rep_array_unsliced<uint> dummy_index, rep_array_unsliced<int> found, int builder) {
    // potential micro memory leak
    auto start = clock_start();
    rep_array_unsliced<block> circuit_input(4);
    rep_array_unsliced<block> circuit_output(1);
    block lookup_values[4];
    memset(lookup_values, 0, 4 * sizeof(block));
    if (party != builder) {
        if (party == prev_party(builder)) {
            lookup_values[0] = key;
        }
        lookup_values[1] = table_2share[h0(&key, log_single_col_len)];
        lookup_values[2] = table_2share[h1(&key, log_single_col_len)];
    }
    circuit_input.from_2shares(prev_party(builder), next_party(builder), lookup_values);
    circuit_input.copy_one(3, dummy_index, 0);
    time_before_circuit = time_from(start);

/* circuit input format (4 blocks)
key | lookup_value_0 | lookup_value_1 | (dummy_index | 96b unused)

output format (1 block)
(index | found | 88b unspecified)
 */ 
    start = clock_start();
    lookup_circuit->compute(circuit_input, circuit_output);
    time_in_circuit = time_from(start);
    start = clock_start();
    block ret;
    circuit_output.reveal_to(prev_party(builder), &ret);
    circuit_output.reveal_to(next_party(builder), &ret);
    found.copy_bytes_from(circuit_output, 1, 4);
    circuit_input.destroy();
    circuit_output.destroy();
    time_after_circuit = time_from(start);
    uint* ret_32s = (uint*)(&ret);
    return ret_32s[0];
}

}

}

