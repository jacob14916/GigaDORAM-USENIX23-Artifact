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

namespace sh_riro 
{
using namespace thread_unsafe;

double time_before_compute;
double time_compute;
double time_after_compute;

enum Role {
    GARBLER_1, GARBLER_2
};

template <typename RealIO>
class SendBufferIO
{
    public:
    RealIO* real_io;
    int buf_len;
    block* buf;
    int counter = 0;
    SendBufferIO(RealIO* real_io, int buf_len) 
    : real_io(real_io), buf_len(buf_len), buf(new block[buf_len]) {}
    ~SendBufferIO() {
        delete[] buf;
    }
    void send_block(block* blocks, int num_blocks) {
        assert(counter + num_blocks <= buf_len);
        memcpy(buf + counter, blocks, num_blocks * sizeof(block));
        counter += num_blocks;
    }
    void flush() {
        real_io->send_block(buf, counter);
        counter = 0;
    }
};

template <typename RealIO>
class RecvBufferIO
{
    public:
    RealIO* real_io;
    int buf_len;
    block* buf;
    int read_counter = 0;
    int recv_counter = 0;
    RecvBufferIO(RealIO* real_io, int buf_len) 
    : real_io(real_io), buf_len(buf_len), buf(new block[buf_len]) {}
    ~RecvBufferIO() {
        delete[] buf;
    }
    void recv_block(block* blocks, int num_blocks) {
        assert(read_counter + num_blocks <= recv_counter);
        memcpy(blocks, buf + read_counter, num_blocks * sizeof(block));
        read_counter += num_blocks;
    }
    void actually_receive(int num_blocks = 0) {
        if (num_blocks == 0) num_blocks = buf_len;
        assert(read_counter == recv_counter);
        real_io->recv_block(buf, num_blocks);
        read_counter = 0;
        recv_counter = num_blocks;
    }
};

// repl_input = eval primary share | eval secondary share | garbler shares

template <typename GarbleIO, typename RegularIO>
void garble(Role which_garbler, BristolFashion_array &circuit_file, HalfGateGenCustomPRG<GarbleIO> *gen, RegularIO *eval_full_io,
                 vector<block> &repl_input_vec, vector<block> &repl_output_vec,
                 PRG *garbler_shared_prg, PRG *g1_eval_shared_prg, PRG *all_shared_prg)
{
    auto start = clock_start();
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

    // key preparation
    block *input_zero_keys = new block[repl_input_length_bits + circuit_output_length_bits];
    garbler_shared_prg->random_block(input_zero_keys, 2 * circuit_input_length_bits);
    // garbler "silent" input trick
    // all_shared_prg->random_block(input_zero_keys + 2 * circuit_input_length_bits, circuit_input_length_bits);
    memset(input_zero_keys + 2 * circuit_input_length_bits, 0, sizeof(block) * circuit_input_length_bits);
    garbler_shared_prg->random_block(input_zero_keys + repl_input_length_bits, circuit_output_length_bits);

    for (int i = 0; i < circuit_input_length_bits; i++)
    {
        // this branch is probably vulnerable to a timing attack
        if (getBitArr(repl_input + 2 * circuit_input_length_blocks, i))
            input_zero_keys[2 * circuit_input_length_bits + i] ^= delta;
    }

    block *circuit_input_zero_keys = new block[circuit_input_length_bits];
    for (int i = 0; i < circuit_input_length_bits; i++)
    {
        circuit_input_zero_keys[i] = input_zero_keys[i] ^ input_zero_keys[i + circuit_input_length_bits] ^
                                     input_zero_keys[i + 2 * circuit_input_length_bits];
    }

    // key sending
    block *eval_shared_keys = new block[circuit_input_length_bits];
    if (which_garbler == GARBLER_1) {
        for (int i = 0; i < circuit_input_length_bits; i++) {
            eval_shared_keys[i] = input_zero_keys[i] ^ (select_mask[getBitArr(repl_input, i)] & delta);
        }
    } else if (which_garbler == GARBLER_2) {
        for (int i = 0; i < circuit_input_length_bits; i++) {
            eval_shared_keys[i] = input_zero_keys[i + circuit_input_length_bits] ^ 
                (select_mask[getBitArr(repl_input, i + circuit_input_length_bits)] & delta);
        }
    }
    eval_full_io->send_block(eval_shared_keys, circuit_input_length_bits);

    // circuit computation
    block *circuit_output_zero_keys = new block[circuit_output_length_bits];
    time_before_compute = time_from(start);
    start = clock_start();
    circuit_file.compute_garbled(circuit_output_zero_keys, circuit_input_zero_keys, gen);
    gen->io->flush();
    time_compute = time_from(start);
    start = clock_start();

    // now set output / process what's received from eval
    // garbler shared output
    for (int i = 0; i < circuit_output_length_bits; i++)
    {
        // if (i < 10) dbg_args(circuit_output_zero_keys[i]);
        setBitArr(repl_output + 2 * circuit_output_length_blocks, i, getLSB(circuit_output_zero_keys[i]));
    }

    if (which_garbler == GARBLER_1) {
        g1_eval_shared_prg->random_block(repl_output, circuit_output_length_blocks);
    } else if (which_garbler == GARBLER_2) {
        eval_full_io->recv_block(repl_output + circuit_output_length_blocks, circuit_output_length_blocks);
    }

    time_after_compute = time_from(start);

    // clean up memory
    delete[] input_zero_keys;
    delete[] eval_shared_keys;
    delete[] circuit_input_zero_keys;
    delete[] circuit_output_zero_keys;
}

template <typename GarbleIO, typename RegularIO>
void eval(BristolFashion_array &circuit_file, HalfGateEva<GarbleIO> *eva, RegularIO *g1_io, RegularIO* g2_io,
               vector<block> &repl_input_vec, vector<block> &repl_output_vec, PRG *g1_eval_shared_prg,
               PRG *all_shared_prg)
{
    auto start = clock_start();
    int repl_input_length_blocks = repl_input_vec.size();
    int repl_output_length_blocks = repl_output_vec.size();
    // block* repl_input = repl_input_vec.data();
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

    block *input_keys = new block[repl_input_length_bits];

    g1_io->recv_block(input_keys, circuit_input_length_bits);
    g2_io->recv_block(input_keys + circuit_input_length_bits, circuit_input_length_bits);
    // all_shared_prg->random_block(input_keys + 2 * circuit_input_length_bits, circuit_input_length_bits);
    memset(input_keys + 2 * circuit_input_length_bits, 0, sizeof(block) * circuit_input_length_bits);

    block *circuit_input_keys = new block[circuit_input_length_bits];
    for (int i = 0; i < circuit_input_length_bits; i++)
    {
        circuit_input_keys[i] =
            input_keys[i] ^ input_keys[i + circuit_input_length_bits] ^ input_keys[i + 2 * circuit_input_length_bits];
    }

    block *circuit_output_keys = new block[circuit_output_length_bits];
    time_before_compute = time_from(start);
    start = clock_start();
    eva->io->actually_receive();
    circuit_file.compute_garbled(circuit_output_keys, circuit_input_keys, eva);
    time_compute = time_from(start);
    start = clock_start();

    // repl_output = g1 eval | g2 eval | g1 g2
    // g1 eval share = random
    // g2 eval share = g1 eval share ^ circuit output colors
    // g1 g2 share = circuit output lambdas
    g1_eval_shared_prg->random_block(repl_output, circuit_output_length_blocks);
    for (int i = 0; i < circuit_output_length_bits; i++)
    {
        setBitArr(repl_output + circuit_output_length_blocks, i, getLSB(circuit_output_keys[i]));
    }
    for (int i = 0; i < circuit_output_length_blocks; i++)
    {
        repl_output[i + circuit_output_length_blocks] ^= repl_output[i];
    }

    // send g2 share to g2
    g2_io->send_block(repl_output + circuit_output_length_blocks, circuit_output_length_blocks);

    time_after_compute = time_from(start);

    // clean up memory
    delete[] input_keys;
    delete[] circuit_input_keys;
    delete[] circuit_output_keys;
}

class Circuit {

BristolFashion_array& circuit_file;
RecvBufferIO<RepNetIO>* rb_io;
HalfGateEva<RecvBufferIO<RepNetIO> > *half_gate_eva;
SendBufferIO<RepNetIO>* sb_io;
HalfGateGenCustomPRG<SendBufferIO<RepNetIO>> *next_gen;
AbandonIO* g2_abandon_io;
HalfGateGenCustomPRG<AbandonIO>* prev_gen;

public:
Circuit(BristolFashion_array& circuit_file)
:circuit_file(circuit_file)
{
    if (party == 1) {
        // todo: make this global
        rb_io = new RecvBufferIO<RepNetIO>(prev_io, 2 * circuit_file.num_and_gate);
        rb_io->actually_receive(3);
        half_gate_eva = new HalfGateEva<RecvBufferIO<RepNetIO> >(rb_io);
    } else if (party == 2) {
        // G2
        g2_abandon_io = new AbandonIO();
        prev_gen = new HalfGateGenCustomPRG<AbandonIO> (g2_abandon_io, next_prg);
    } else if (party == 3) {
        // G1
        sb_io = new SendBufferIO<RepNetIO> (next_io, 2 * circuit_file.num_and_gate);
        next_gen = new HalfGateGenCustomPRG<SendBufferIO<RepNetIO>> (sb_io, prev_prg);
        sb_io->flush();
    }
}

void compute (rep_array_unsliced<block> &in, rep_array_unsliced<block> &out) {
    vector<block> in_fixed = in.to_fixed();
    vector<block> out_fixed(3 * out.length_Ts());
    if (party == 1) {
        // todo: make this global
        eval(circuit_file, half_gate_eva, prev_io, next_io, in_fixed, out_fixed, prev_prg, shared_prg);
    } else if (party == 2) {
        // G2
        garble(GARBLER_2, circuit_file, prev_gen, prev_io, in_fixed, out_fixed, next_prg, nullptr, shared_prg);
    } else if (party == 3) {
        garble(GARBLER_1, circuit_file, next_gen, next_io, in_fixed, out_fixed, prev_prg, next_prg, shared_prg);
    }
    out.of_fixed(out_fixed);
}
};


} // namespace riro

} // namespace emp