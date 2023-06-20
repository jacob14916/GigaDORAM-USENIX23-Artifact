#pragma once


#include "debug.h"
#include "emp-tool/emp-tool.h"
#include "globals.h"
#include <iostream>
#include <stdio.h>
#include <vector>

using namespace std;
namespace emp
{

template <typename Share> class BristolFashion_better
{
  public:
    int num_gate = 0, num_wire = 0, num_input = 0, num_output = 0;
    vector<int> gates;
    vector<vector<int>> mand_input_lists;
    vector<vector<int>> mand_output_lists;

    int max_gate_input_wires = 0, max_gate_output_wires = 0;

    BristolFashion_better(string file)
    {
        this->from_file_extended(file);
    }

    BristolFashion_better(char *file)
    {
        this->from_file_extended(file);
    }

    void from_file_extended(string filename)
    {
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

        gates.resize(num_gate * 4);

        string gate_type; // todo might be faster with char...
        int gate_input_wires, gate_output_wires;
        for (int i = 0; i < num_gate; i++) // for gate
        {
            file >> gate_input_wires >> gate_output_wires;
            if (gate_input_wires == 2)
            {
                file >> gates[4 * i] >> gates[4 * i + 1] >> gates[4 * i + 2] >> gate_type;
                if (gate_type == "AND")
                {
                    gates[4 * i + 3] = AND_GATE;
                }
                else if (gate_type == "XOR")
                {
                    gates[4 * i + 3] = XOR_GATE;
                }
            }
            else if (gate_input_wires == 1)
            { // INV gate
                file >> gates[4 * i] >> gates[4 * i + 2] >> gate_type;
                gates[4 * i + 3] = NOT_GATE;
                assert(gate_type == "INV");
            }
            else
            { // MAND gate

                gates[4 * i] = mand_input_lists.size();

                gates[4 * i + 3] = MAND_GATE;
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

    ~BristolFashion_better()
    {
    }

    template <typename Exec> void compute(Share *out, const Share *in, Exec *circ_exec, int num_in_parallel = 1)
    {
        Share *all_wires = new Share[num_in_parallel * num_wire];
        Share *mand_input_buffer = new Share[num_in_parallel * max_gate_input_wires];
        Share *mand_output_buffer = new Share[num_in_parallel * max_gate_output_wires];
        for (int j = 0; j < num_in_parallel; j++)
        {
            memcpy(all_wires + j * num_wire, in + j * num_input, num_input * sizeof(Share));
        }

        // TODO: Cache optimization: do XOR gates one parallel copy at a time
        // TODO: But MAND gates for all the copies at once

        for (int i = 0; i < num_gate; ++i)
        {
            if (gates[4 * i + 3] == MAND_GATE)
            {
                // todo: malloc within the loop -- optimize -- can just malloc the max
                // size inputs and outputs (we already processed through)
                vector<int> &in_list = mand_input_lists[gates[4 * i]];
                vector<int> &out_list = mand_output_lists[gates[4 * i]];
                int gate_input_wires = in_list.size();
                int gate_output_wires = out_list.size();
                for (int j = 0; j < num_in_parallel; j++)
                {
                    Share *wires = all_wires + j * num_wire;
                    for (int k = 0; k < gate_input_wires; k++)
                    {
                        mand_input_buffer[j * gate_input_wires + k] = wires[in_list[k]];
                    }
                    circ_exec->start_mand_gate(mand_input_buffer + j * gate_input_wires,
                                               mand_input_buffer + j * gate_input_wires +
                                                   gate_output_wires, // gate_output_wires = gate_input_wires/2
                                               mand_output_buffer + j * gate_output_wires, gate_output_wires);
                }
                next_io->flush();
                for (int j = 0; j < num_in_parallel; j++)
                {
                    circ_exec->finish_mand_gate(mand_input_buffer + j * gate_input_wires,
                                                mand_input_buffer + j * gate_input_wires +
                                                    gate_output_wires, // gate_output_wires = gate_input_wires/2
                                                mand_output_buffer + j * gate_output_wires, gate_output_wires);
                    Share *wires = all_wires + j * num_wire;
                    for (int k = 0; k < gate_output_wires; k++)
                    {
                        wires[out_list[k]] = mand_output_buffer[j * gate_output_wires + k];
                    }
                }
            }
            else
            {
                for (int j = 0; j < num_in_parallel; j++) // todo: when we make rep_share vectorized execute all gates
                                                          // up to MAND (or at least XORs together)
                {
                    Share *wires = all_wires + j * num_wire;
                    if (gates[4 * i + 3] == AND_GATE)
                    {
                        wires[gates[4 * i + 2]] = circ_exec->and_gate(wires[gates[4 * i]], wires[gates[4 * i + 1]]);
                    }
                    else if (gates[4 * i + 3] == XOR_GATE)
                    {
                        wires[gates[4 * i + 2]] = circ_exec->xor_gate(wires[gates[4 * i]], wires[gates[4 * i + 1]]);
                    }
                    else
                    {
                        // dbg_args(4 * i + 2, gates[4 * i + 2], wires[gates[4 * i + 2]], gates[4 * i],
                        //          wires[gates[4 * i]], gates.size()); //still no bound check on wires
                        wires[gates[4 * i + 2]] = circ_exec->not_gate(wires[gates[4 * i]]);
                    }
                }
            }
        }

        for (int j = 0; j < num_in_parallel; j++)
        {
            memcpy(out + j * num_output, all_wires + j * num_wire + (num_wire - num_output),
                   num_output * sizeof(Share));
        }
        delete[] mand_input_buffer;
        delete[] mand_output_buffer;
        delete[] all_wires; // this was not deleted, but will likely cause a huge mem
                            // leak othereise
    }

    /*call to eval AES with shared key:
        processes circuit

    */
};
} // namespace emp