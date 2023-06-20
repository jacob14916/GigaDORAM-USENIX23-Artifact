#pragma once


#include "debug.h"
#include "emp-tool/emp-tool.h"
#include "globals.h"
#include "sh_rep_array.h"
#include <iostream>
#include <stdio.h>
#include <vector>

using namespace std;
namespace emp
{

enum gate_type {
    AND, XOR_, INV, MAND, EQW, ERR
};

struct bristol_fashion_gate {

    gate_type type = ERR;
    int wire1 = -1;
    int wire2 = -1;
    int wire3 = -1;
    int mand_index = -1;
    
};

ostream& operator<< (ostream& stream, const bristol_fashion_gate& gate) {
    return stream << gate.type << ' ' << gate.wire1 << ' ' << gate.wire2 << ' ' << gate.wire3 << ' ' << gate.mand_index;
}

void compute_static(BristolFashion_array *this_, rep_array_unsliced<block> out, rep_array_unsliced<block> in, 
    int num_parallel, SHRepArray *rep_exec);

class BristolFashion_array
{
  public:
    int num_gate = 0, num_and_gate = 0, num_wire = 0, num_input = 0, num_output = 0;
    vector<bristol_fashion_gate> gates;
    vector<vector<int>> mand_input_lists;
    vector<vector<int>> mand_output_lists;
    vector<block> garbled_wires;

    int max_gate_input_wires = 0, max_gate_output_wires = 0;

    BristolFashion_array(string file)
    {
        this->from_file_extended(file);
    }

    BristolFashion_array(char *file)
    {
        this->from_file_extended(file);
    }

    void from_file_extended(string filename)
    {

        if (!file_exists(filename))
        {
            cerr << "File doesn't exist: " << filename << '\n';
            exit(1);
        }
        ifstream file(filename);

        file >> num_gate >> num_wire;
        int num_input_values, num_output_values;

        file >> num_input_values;
        for (int i = 0; i < num_input_values; i++)
        {
            int tmp;
            file >> tmp;
            num_input += tmp;
        }
        file >> num_output_values;
        for (int i = 0; i < num_output_values; i++)
        {
            int tmp;
            file >> tmp;
            num_output += tmp;
        }

        gates.resize(num_gate);

        string gate_type; // todo might be faster with char...
        int gate_input_wires, gate_output_wires;
        for (int i = 0; i < num_gate; i++) // for gate
        {
            file >> gate_input_wires >> gate_output_wires;
            if (gate_input_wires == 2)
            {
                file >> gates[i].wire1 >> gates[i].wire2 >> gates[i].wire3 >> gate_type;
                if (gate_type == "AND" || gate_type == "MAND")
                {
                    gates[i].type = AND;
                    num_and_gate++;
                }
                else if (gate_type == "XOR")
                {
                    gates[i].type = XOR_;
                } else {
                    assert("Unrecognized gate" && false);
                }
            }
            else if (gate_input_wires == 1)
            { // INV gate
                file >> gates[i].wire1 >> gates[i].wire2 >> gate_type;
                if (gate_type == "INV") {
                    gates[i].type = INV;
                } else if (gate_type == "EQW") {
                    gates[i].type = EQW;
                } else {
                    assert("Unrecognized gate" && false);
                }
            }
            else
            { // MAND gate
                gates[i].mand_index = mand_input_lists.size();
                gates[i].type = MAND;
                vector<int> input_list(gate_input_wires), output_list(gate_output_wires);

                for (int j = 0; j < gate_input_wires; j++)
                {
                    file >> input_list[j];
                }
                for (int j = 0; j < gate_output_wires; j++)
                {
                    file >> output_list[j];
                }
                mand_input_lists.push_back(input_list);
                mand_output_lists.push_back(output_list);
                max_gate_input_wires = max(max_gate_input_wires, gate_input_wires);
                max_gate_output_wires = max(max_gate_output_wires, gate_output_wires);
                file >> gate_type;
                assert(gate_type == "MAND");
            }
        }
    }

    ~BristolFashion_array()
    {
    }

    template<typename Circ_exec, typename S, int num_parallel_Ss>
    void compute_internal(rep_array_unsliced<block> &out, rep_array_unsliced<block> &in,  int num_parallel, Circ_exec *circ_exec)
    {
        // slice
        assert(num_parallel <= 1024);
        assert(num_parallel_Ss == (num_parallel + (8 * sizeof(S) - 1)) / (8 * sizeof(S)));
        // todo: support circuits that aren't block -> block
        assert(num_input % 128 == 0);
        uint num_input_blocks = num_input / 128;
        // round input size up
        rep_array_unsliced<block> in_wide(num_parallel_Ss * 8 * sizeof(S) * num_input_blocks);
        in_wide.copy_bytes_from(in, in.length_bytes);
        rep_array_sliced<S, num_parallel_Ss> all_wires(in_wide, num_wire);
        // compute as though 1 wide 
        rep_array_sliced<S, num_parallel_Ss> mand_input_buffer(max_gate_input_wires);
        rep_array_sliced<S, num_parallel_Ss> mand_output_buffer(max_gate_output_wires);

        for (bristol_fashion_gate& gate : gates) { 
            if (gate.type == XOR_) {
                all_wires.xor_indices(gate.wire1, gate.wire2, gate.wire3);
            } else if (gate.type == MAND)
            {
                vector<int> &in_list = mand_input_lists[gate.mand_index];
                vector<int> &out_list = mand_output_lists[gate.mand_index];

                int gate_input_wires = in_list.size();
                int gate_output_wires = out_list.size();
                for (int j = 0; j < gate_input_wires; j++) {
                    int input_wire = in_list[j];
                    all_wires.repcpy_sliced(mand_input_buffer, 1, input_wire, j);
                }
                rep_array_sliced<S, num_parallel_Ss> mand_output_window = 
                                    mand_output_buffer.window_sliced(gate_output_wires, 0);
                circ_exec->mand_gate(mand_input_buffer.window_sliced(gate_input_wires/2, 0),
                                    mand_input_buffer.window_sliced(gate_input_wires/2, gate_input_wires/2),
                                    mand_output_window);
                for (int j = 0; j < gate_output_wires; j++) {
                    int output_wire = out_list[j];
                    mand_output_window.repcpy_sliced(all_wires, 1, j, output_wire);
                }
            } else if (gate.type == AND) {
                rep_array_sliced<S, num_parallel_Ss> mand_output_window = all_wires.window_sliced(1, gate.wire3);
                circ_exec->mand_gate(all_wires.window_sliced(1, gate.wire1), 
                    all_wires.window_sliced(1, gate.wire2), mand_output_window);
            } else if (gate.type == INV) {
                all_wires.not_indices(gate.wire1, gate.wire2);
            } else if (gate.type == EQW) {
                all_wires.copy_indices(gate.wire1, gate.wire2);
            } else {
                assert("Unknown/unsupported gate type" && false);
            }
        }

        rep_array_sliced<S, num_parallel_Ss> out_wires = all_wires.window_sliced(num_output, num_wire - num_output);
        // actually let's not resize / write past the end of out
        assert(num_output % 128 == 0);
        int num_output_blocks = num_output / 128;
        rep_array_unsliced<block>out_wide(num_parallel_Ss * 8 * sizeof(S) * num_output_blocks);

        out_wires.unslice(out_wide);
        out.copy_bytes_from(out_wide, num_parallel * num_output / 8);

        all_wires.destroy();
        in_wide.destroy();
        mand_input_buffer.destroy();
        mand_output_buffer.destroy();
        out_wide.destroy();

    }

template<typename Circ_exec>
    void compute(rep_array_unsliced<block> out, rep_array_unsliced<block> in, int num_parallel,
    Circ_exec* rep_exec) {
        if (num_parallel <= 8) {
            compute_internal<Circ_exec, uint8_t, 1>(out, in, num_parallel, rep_exec);
        } else if (num_parallel <= 16) {
            compute_internal<Circ_exec, uint16_t, 1>(out, in, num_parallel, rep_exec);
        } else if (num_parallel <= 32) {
            compute_internal<Circ_exec, uint32_t, 1>(out, in, num_parallel, rep_exec);
        } else if (num_parallel <= 64) {
            compute_internal<Circ_exec, uint64_t, 1>(out, in, num_parallel, rep_exec);
        } else {
            int chunk = 0;
            int num_input_128s = num_input / 128;
            int chunk_input = num_input_128s * 1024;
            int num_output_128s = num_output / 128;
            int chunk_output = num_output_128s * 1024;
            for (; chunk < num_parallel/1024; chunk++) {
                rep_array_unsliced<block> in_window = in.window(chunk * chunk_input, chunk_input);
                rep_array_unsliced<block> out_window = out.window(chunk * chunk_output, chunk_output);
                compute_internal<Circ_exec, block, 8>(out_window, in_window, 1024, rep_exec);
            }
            if (chunk * 1024 != num_parallel) {
                int leftover = num_parallel - 1024 * chunk;
                assert(leftover > 0 && leftover < 1024);
                int num_parallel_blocks = (leftover + 127) / 128;
                rep_array_unsliced<block> in_window = in.window(chunk * chunk_input, leftover * num_input_128s);
                rep_array_unsliced<block> out_window = out.window(chunk * chunk_output, leftover * num_output_128s);
                switch (num_parallel_blocks) {
                case 1: compute_internal<Circ_exec, block, 1>(out_window, in_window, leftover, rep_exec);
                break;
                case 2: compute_internal<Circ_exec, block, 2>(out_window, in_window, leftover, rep_exec);
                break;
                case 3: compute_internal<Circ_exec, block, 3>(out_window, in_window, leftover, rep_exec);
                break;
                case 4: compute_internal<Circ_exec, block, 4>(out_window, in_window, leftover, rep_exec);
                break;
                case 5: compute_internal<Circ_exec, block, 5>(out_window, in_window, leftover, rep_exec);
                break;
                case 6: compute_internal<Circ_exec, block, 6>(out_window, in_window, leftover, rep_exec);
                break;
                case 7: compute_internal<Circ_exec, block, 7>(out_window, in_window, leftover, rep_exec);
                break;
                case 8: compute_internal<Circ_exec, block, 8>(out_window, in_window, leftover, rep_exec);
                break;
                default:
                // This should never happen
                cerr << "Error: leftover parallel blocks not in [1,8]: " << num_parallel_blocks << '\n';
                exit(1);
                }
            }
        }
    }

    void compute_multithreaded(rep_array_unsliced<block> out, rep_array_unsliced<block> in, uint num_parallel_copies_of_circuit_not_threads) {
        // TODO: better logic to spin up an optimal number of threads, rather than all of them
        uint copies_per_thread = num_parallel_copies_of_circuit_not_threads / NUM_THREADS;
        vector<thread> threads;
        for (uint i = 0; i < NUM_THREADS; i++) {
            uint thread_section_start_copies = i * copies_per_thread;
            uint thread_section_length_copies = -1;
            if (i == NUM_THREADS - 1) {
                thread_section_length_copies = num_parallel_copies_of_circuit_not_threads - thread_section_start_copies;
            } else {
                thread_section_length_copies = copies_per_thread;
            }
            assert (thread_section_length_copies > 0);
            assert (num_input % 128 == 0);
            assert (num_output % 128 == 0);
            uint num_input_blocks = num_input / 128;
            uint num_output_blocks = num_output / 128;
            rep_array_unsliced<block> out_section = out.window(thread_section_start_copies * num_output_blocks, 
                thread_section_length_copies * num_output_blocks);
            rep_array_unsliced<block> in_section = in.window(thread_section_start_copies * num_input_blocks, 
                thread_section_length_copies * num_input_blocks);
            // this calls the thread constructor and stores in the back of the vector
            threads.emplace_back(compute_static, this, out_section, in_section, thread_section_length_copies, rep_execs[i]);
        }
        for (uint i = 0; i < NUM_THREADS; i++) {
            threads[i].join();
        }
    }

    template<typename Circ_exec>
    void compute_garbled (block * out, const block * in, Circ_exec* circ_exec) {
        if (garbled_wires.size() == 0) {
            garbled_wires.resize(num_wire);
        }
        memcpy(garbled_wires.data(), in, num_input * sizeof(block));
        for (bristol_fashion_gate& gate : gates) {
            if (gate.type == AND) {
                garbled_wires[gate.wire3] = circ_exec->and_gate(garbled_wires[gate.wire1], garbled_wires[gate.wire2]);
            } else if (gate.type == XOR_) {
                garbled_wires[gate.wire3] = circ_exec->xor_gate(garbled_wires[gate.wire1], garbled_wires[gate.wire2]);
            } else if (gate.type == INV) {
                garbled_wires[gate.wire2] = circ_exec->not_gate(garbled_wires[gate.wire1]);
            } else if (gate.type == EQW) {
                garbled_wires[gate.wire2] = garbled_wires[gate.wire1];
            } else {
                assert("Unsupported gate type" && false);
            }
        }
        memcpy(out, garbled_wires.data() + (num_wire - num_output), num_output * sizeof(block));
    }

};

void compute_static(BristolFashion_array *this_, rep_array_unsliced<block> out, rep_array_unsliced<block> in, 
    int num_parallel, SHRepArray *rep_exec)
    {
        this_->compute(out, in, num_parallel, rep_exec);
    }


} // namespace emp