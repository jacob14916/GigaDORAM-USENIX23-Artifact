#pragma once

#include <filesystem>
#include <iostream>
#include <vector>

#include "emp-tool/emp-tool.h"
#include "block.h"
#include "debug.h"
#include "halfgate_gen_prg.h"
#include "globals.h"
#include "rep_array_unsliced.h"
#include "bristol_fashion_array.h"
#include "hash_io.h"

using namespace std;

namespace emp
{

namespace riro 
{

double time_create_eva;
double time_eval;
double time_eval_before_compute;
double time_eval_compute;

enum Role {
    GARBLER_1, GARBLER_2
};

const int MALICIOUS = 1;

void create_wire_key_commitment(block &key, block &delta, block *randomness, block *hashes)
{
    block commitment_inputs[4];
    // possible improvement: un-unroll this
    commitment_inputs[0] = key;
    commitment_inputs[1] = randomness[0];
    commitment_inputs[2] = key ^ delta;
    commitment_inputs[3] = randomness[1];

    hashes[0] = Hash::hash_for_block(commitment_inputs, 32);
    hashes[1] = Hash::hash_for_block(commitment_inputs + 2, 32);
}

void decommit(bool bit, block &key, block &delta, block *randomness, block *commitment_values, bool debug_print = false)
{
    if (bit)
    {
        commitment_values[0] = key ^ delta;
        commitment_values[1] = randomness[1];
    }
    else
    {
        commitment_values[0] = key;
        commitment_values[1] = randomness[0];
    }
    if (debug_print)
    {
        // cerr << party << ": sending decommitment " << bit << " " << commitment_values[0] << ", " << commitment_values[1]
        //    << '\n';
    }
}

block check_decommit(bool bit, block *hashes, block *commitment_values)
{
    block check_hash = Hash::hash_for_block(commitment_values, 32);
    if (blocksEqual(check_hash, hashes[bit]))
    {
        return commitment_values[0];
    }
    cerr << party << ": Failed to decommit a " << bit << ": " << check_hash << " != " << hashes[bit] << '\n';
    assert(false);
}

// repl_input = eval primary share | eval secondary share | garbler shares

template <typename IO, typename FIO>
void garble(Role which_garbler, BristolFashion_array &circuit_file, HalfGateGenCustomPRG<IO> *gen, IO *eval_io, FIO *eval_full_io,
                 vector<block> &repl_input_vec, vector<block> &repl_output_vec,
                 PRG *garbler_shared_prg, PRG *g1_eval_shared_prg, PRG *all_shared_prg)
{
    int repl_input_length_blocks = repl_input_vec.size();
    int repl_output_length_blocks = repl_output_vec.size();
    block* repl_input = repl_input_vec.data();
    block* repl_output = repl_output_vec.data();

    // cerr << "Starting RIRO garble\n";

    // using sizeof(block) = 128 bits or 16 bytes
    // and assuming circuit does blocks -> blocks
    int repl_input_length_bits = 128 * repl_input_length_blocks;
    int circuit_input_length_blocks = repl_input_length_blocks / 3;
    int circuit_input_length_bits = 128 * circuit_input_length_blocks;

    // int repl_output_length_bits = 128 * repl_output_length_blocks; // unused
    int circuit_output_length_blocks = repl_output_length_blocks / 3;
    int circuit_output_length_bits = 128 * circuit_output_length_blocks;

    block delta = gen->delta;
    // create keys for all 3 replicated inputs
    // format: x1 x2 x3
    block *input_zero_keys = new block[repl_input_length_bits + circuit_output_length_bits];
    garbler_shared_prg->random_block(input_zero_keys, 2 * circuit_input_length_bits);
    // garbler "silent" input trick
    all_shared_prg->random_block(input_zero_keys + 2 * circuit_input_length_bits, circuit_input_length_bits);
    garbler_shared_prg->random_block(input_zero_keys + repl_input_length_bits, circuit_output_length_bits);

    for (int i = 0; i < circuit_input_length_bits; i++)
    {
        // this branch is probably vulnerable to a timing attack
        if (getBitArr(repl_input + 2 * circuit_input_length_blocks, i))
            input_zero_keys[2 * circuit_input_length_bits + i] ^= delta;
    }

    // send commitments
    // shoot, are we working in the ROM or not?
    // I think we are, so I'll just use hashes
    // Naor commitments also an option
    block *input_key_commitment_pads = new block[4 * circuit_input_length_bits + 2 * circuit_output_length_bits];
    garbler_shared_prg->random_block(input_key_commitment_pads,
                                    4 * circuit_input_length_bits + 2 * circuit_output_length_bits);

    block *input_key_commitments = new block[4 * circuit_input_length_bits + 2 * circuit_output_length_bits];
    // No need to permute anything
    for (int i = 0; i < 2 * circuit_input_length_bits; i++)
    {
        create_wire_key_commitment(input_zero_keys[i], delta, input_key_commitment_pads + 2 * i,
                                   input_key_commitments + 2 * i);
    }

    block *output_pad_key_commitment_pads = input_key_commitment_pads + 4 * circuit_input_length_bits;
    block *output_pad_key_commitments = input_key_commitments + 4 * circuit_input_length_bits;

    // no need to permute these either
    block *output_pad_zero_keys = input_zero_keys + repl_input_length_bits;
    for (int i = 0; i < circuit_output_length_bits; i++)
    {
        create_wire_key_commitment(output_pad_zero_keys[i], delta, output_pad_key_commitment_pads + 2 * i,
                                   output_pad_key_commitments + 2 * i);
    }

    eval_io->send_block(input_key_commitments, 4 * circuit_input_length_bits + 2 * circuit_output_length_bits);

    block *circuit_input_zero_keys = new block[circuit_input_length_bits];
    for (int i = 0; i < circuit_input_length_bits; i++)
    {
        circuit_input_zero_keys[i] = input_zero_keys[i] ^ input_zero_keys[i + circuit_input_length_bits] ^
                                     input_zero_keys[i + 2 * circuit_input_length_bits];
    }

    block *output_pad_values = repl_output;
    // primary garbler sends output pad
    if (which_garbler == GARBLER_1)
    {

        g1_eval_shared_prg->random_block(output_pad_values, circuit_output_length_blocks);
        block* commitment_values = new block[2 * circuit_input_length_bits + 2 * circuit_output_length_bits];

        // cerr << party << ": ikcp[0], ikcp[1] = " << input_key_commitment_pads[0] << ", " << input_key_commitment_pads[1]
             // << '\n';
        // cerr << party << ": izk[0], izk[1] = " << input_zero_keys[0] << ", " << input_zero_keys[1] << '\n';
        // decommit evaluator and G1 shared input
        for (int i = 0; i < circuit_input_length_bits; i++)
        {
            decommit(getBitArr(repl_input, i), input_zero_keys[i], delta, input_key_commitment_pads + 2 * i, commitment_values + 2 * i);
        }

        // decommit output pad
        for (int i = 0; i < circuit_output_length_bits; i++)
        {
            decommit(getBitArr(output_pad_values, i), output_pad_zero_keys[i], delta,
                     output_pad_key_commitment_pads + 2 * i, commitment_values + 2 * (i + circuit_input_length_bits));
        }
        eval_io->send_block(commitment_values, 2 * circuit_input_length_bits + 2 * circuit_output_length_bits);
        delete[] commitment_values;
    }
    else
    {
        block* commitment_values = new block[2 * circuit_input_length_bits];
        // decommit evaluator and G2 shared input
        for (int i = circuit_input_length_bits; i < 2 * circuit_input_length_bits; i++)
        {
            // use eval_full_io because eval_io is a HashIO
            decommit(getBitArr(repl_input, i), input_zero_keys[i], delta, input_key_commitment_pads + 2 * i,
                     commitment_values + 2 * (i - circuit_input_length_bits));
        }
        eval_full_io->send_block(commitment_values, 2 * circuit_input_length_bits);
    }

    // ready to garble the circuit!
    block *circuit_output_zero_keys = new block[circuit_output_length_bits];
    circuit_file.compute_garbled(circuit_output_zero_keys, circuit_input_zero_keys, gen);

    if (which_garbler == GARBLER_2)
    {
        eval_io->flush();
    }

    // now set output / process what's received from eval
    // garbler shared output
    for (int i = 0; i < circuit_output_length_bits; i++)
    {
        // if (i < 10) dbg_args(circuit_output_zero_keys[i]);
        setBitArr(repl_output + 2 * circuit_output_length_blocks, i, getLSB(circuit_output_zero_keys[i]));
    }

    if (which_garbler == GARBLER_2)
    {
        block *g2_padded_output_keys = new block[circuit_output_length_bits];
        eval_full_io->recv_block(g2_padded_output_keys, circuit_output_length_bits);
        for (int i = 0; i < circuit_output_length_bits; i++)
        {
            block zero_or_delta = g2_padded_output_keys[i] ^ output_pad_zero_keys[i] ^ circuit_output_zero_keys[i];
            if (blocksEqual(zero_or_delta, zero_block))
            {
                setBitArr(repl_output + circuit_output_length_blocks, i, getLSB(circuit_output_zero_keys[i]));
            }
            else if (blocksEqual(zero_or_delta, delta))
            {
                setBitArr(repl_output + circuit_output_length_blocks, i, 1 ^ getLSB(circuit_output_zero_keys[i]));
            }
            else
            {
                cerr << "Couldn't interpret garbler 2 output key: " << zero_or_delta << " not equal to zero or "
                     << delta << '\n';
                assert(false);
            }
        }
        delete[] g2_padded_output_keys;
    }

    // clean up memory
    delete[] input_zero_keys;
    delete[] input_key_commitment_pads;
    delete[] input_key_commitments;
    delete[] circuit_input_zero_keys;
    delete[] circuit_output_zero_keys;
}

template <typename IO, typename HCIO>
void eval(BristolFashion_array &circuit_file, HalfGateEva<HCIO> *eva, HCIO *g1_io, IO *g1_nohash_io, IO *g2_io, 
               vector<block> &repl_input_vec, vector<block> &repl_output_vec, PRG *g1_eval_shared_prg,
               PRG *all_shared_prg)
{
    auto start = clock_start();
    int repl_input_length_blocks = repl_input_vec.size();
    int repl_output_length_blocks = repl_output_vec.size();
    block* repl_input = repl_input_vec.data();
    block* repl_output = repl_output_vec.data();

    // cerr << "Starting RIRO eval\n";

    // using sizeof(block) = 128 bits or 16 bytes
    // and assuming circuit does blocks -> blocks
    int repl_input_length_bits = 128 * repl_input_length_blocks;
    int circuit_input_length_blocks = repl_input_length_blocks / 3;
    int circuit_input_length_bits = 128 * circuit_input_length_blocks;

    // int repl_output_length_bits = 128 * repl_output_length_blocks;
    int circuit_output_length_blocks = repl_output_length_blocks / 3;
    int circuit_output_length_bits = 128 * circuit_output_length_blocks;

    block *input_keys = new block[repl_input_length_bits + circuit_output_length_bits];

    all_shared_prg->random_block(input_keys + 2 * circuit_input_length_bits, circuit_input_length_bits);

    block *input_key_commitments = new block[4 * circuit_input_length_bits + 2 * circuit_output_length_bits];
    g1_io->recv_block(input_key_commitments, 4 * circuit_input_length_bits + 2 * circuit_output_length_bits);
    block *output_pad_key_commitments = input_key_commitments + 4 * circuit_input_length_bits;

    block* g1_commitment_openings = new block[2 * circuit_input_length_bits + 2 * circuit_output_length_bits];
    g1_nohash_io->recv_block(g1_commitment_openings, 2 * circuit_input_length_bits + 2 * circuit_output_length_bits);
    block* g2_commitment_openings = new block[2 * circuit_input_length_bits];
    g2_io->recv_block(g2_commitment_openings, 2 * circuit_input_length_bits);

    // cerr << "Get decommitments for G1 share\n";
    for (int i = 0; i < circuit_input_length_bits; i++)
    {
        input_keys[i] = check_decommit(getBitArr(repl_input, i), input_key_commitments + 2 * i, 
            g1_commitment_openings + 2 * i);
    }

    // cerr << "Get decommitments for G2 share\n";
    for (int i = circuit_input_length_bits; i < 2 * circuit_input_length_bits; i++)
    {
        input_keys[i] = check_decommit(getBitArr(repl_input, i), input_key_commitments + 2 * i, 
            g2_commitment_openings + 2 * (i - circuit_input_length_bits));
    }

    block *output_pad_values = repl_output;
    g1_eval_shared_prg->random_block(output_pad_values, circuit_output_length_blocks);

    // cerr << "Get decommitments for output pad\n";
    for (int i = 0; i < circuit_output_length_bits; i++)
    {
        input_keys[i + repl_input_length_bits] =
            check_decommit(getBitArr(output_pad_values, i), output_pad_key_commitments + 2 * i, 
                g1_commitment_openings + 2 * (i + circuit_input_length_bits));
    }

    block *circuit_input_keys = new block[circuit_input_length_bits];
    for (int i = 0; i < circuit_input_length_bits; i++)
    {
        circuit_input_keys[i] =
            input_keys[i] ^ input_keys[i + circuit_input_length_bits] ^ input_keys[i + 2 * circuit_input_length_bits];
    }

    block *circuit_output_keys = new block[circuit_output_length_bits];
    time_eval_before_compute = time_from(start);
    start = clock_start();
    circuit_file.compute_garbled(circuit_output_keys, circuit_input_keys, eva);
    time_eval_compute = time_from(start);

    // discuss upgrading security of hashes later
    block g1_hash, g2_hash;
    g1_hash = g1_io->digest();
    g2_io->recv_block(&g2_hash, 1);

    if (!blocksEqual(g1_hash, g2_hash))
    {
        cerr << "Eval failed: " << g1_hash << " != " << g2_hash << '\n';
        assert(false);
    }

    // send padded keys to g2
    block *g2_padded_output_keys = new block[circuit_output_length_bits];
    for (int i = 0; i < circuit_output_length_bits; i++)
    {
        g2_padded_output_keys[i] = circuit_output_keys[i] ^ input_keys[i + repl_input_length_bits];
    }
    g2_io->send_block(g2_padded_output_keys, circuit_output_length_bits);

    // repl_output = g1 eval | g2 eval | g1 g2
    // g1 eval share has already been set
    // g2 eval share = g1 eval share ^ circuit output colors
    for (int i = 0; i < circuit_output_length_bits; i++)
    {
        setBitArr(repl_output + circuit_output_length_blocks, i, getLSB(circuit_output_keys[i]));
    }
    for (int i = 0; i < circuit_output_length_blocks; i++)
    {
        repl_output[i + circuit_output_length_blocks] ^= output_pad_values[i];
    }

    // clean up memory
    delete[] input_keys;
    delete[] input_key_commitments;
    delete[] circuit_input_keys;
    delete[] circuit_output_keys;
    delete[] g2_padded_output_keys;
}

void compute (BristolFashion_array& circuit_file, rep_array_unsliced<block> &in, rep_array_unsliced<block> &out) {
    vector<block> in_fixed = in.to_fixed();
    vector<block> out_fixed(3 * out.length_Ts());
    if (party == 1) {
        HashRecvIO<RepNetIO> g1_hash_io(prev_io);
        auto start = clock_start();
        HalfGateEva<HashRecvIO<RepNetIO>> half_gate_eva(&g1_hash_io);
        time_create_eva = time_from(start);
        start = clock_start();
        eval(circuit_file, &half_gate_eva, &g1_hash_io, prev_io, next_io, in_fixed, out_fixed, prev_prg, shared_prg);
        time_eval = time_from(start);
    } else if (party == 2) {
        // G2
        HashSendIO<RepNetIO> g2_hash_io(prev_io);
        HalfGateGenCustomPRG<HashSendIO<RepNetIO>> prev_gen(&g2_hash_io, next_prg);
        garble(GARBLER_2, circuit_file, &prev_gen, &g2_hash_io, prev_io, in_fixed, out_fixed, next_prg, prev_prg, shared_prg);
    } else if (party == 3) {
        // G1
        HalfGateGenCustomPRG<RepNetIO> next_gen(next_io, prev_prg);
        garble(GARBLER_1, circuit_file, &next_gen, next_io, next_io, in_fixed, out_fixed, prev_prg, next_prg, shared_prg);
    }
    out.of_fixed(out_fixed);
}


} // namespace riro

} // namespace emp