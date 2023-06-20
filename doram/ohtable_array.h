#pragma once

#include <vector>
#include "emp-tool/emp-tool.h"
#include "optimal_cht.h"
#include "sh_shuffle_array.h"
#include "sh_rep_array.h"
#include "bristol_fashion_array.h"
#include "utils.h"
#include "globals.h"

namespace emp
{

// couldn't put this in utils because of include order
uint prf_key_size_blocks() {
    assert(prf_circuit != nullptr);
    uint bits_per_block = 8 * sizeof(block); // 128
    assert(prf_circuit->num_input % bits_per_block == 0);
    return prf_circuit->num_input/bits_per_block - 1;
}

double time_before_cht, time_in_cht, time_after_cht, time_ite;

struct OHTableParams
{
    uint num_elements = UINT_MAX;
    uint num_dummies = UINT_MAX;
    uint stash_size = UINT_MAX;
    int builder = -1;
    uint cht_log_single_col_len = UINT_MAX;
    uint key_size_blocks = UINT_MAX;

    void validate() const {
        assert(num_elements != UINT_MAX);
        assert(num_dummies != UINT_MAX);
        assert(stash_size != UINT_MAX);
        assert(builder != -1);
        assert(cht_log_single_col_len != UINT_MAX);
    }

    uint total_size() const {
        return num_elements + num_dummies;
    }

    uint cht_full_table_length() const {
        return 2 << cht_log_single_col_len;
    }
};

ostream& operator<<(ostream& stream, const OHTableParams& params) {
    stream << "OHTable {\n";
    stream << "size: " << params.total_size() << '\n';
    stream << "num_elements: " << params.num_elements << '\n';
    stream << "num_dummies: " << params.num_dummies << '\n';
    stream << "stash_size: " << params.stash_size << '\n';
    stream << "builder: " << params.builder << '\n';
    stream << "cht_log_single_col_len: " << params.cht_log_single_col_len << '\n';
    return stream << "}\n";
}

class OHTable_array
{
    // change to private after debuggine
public:
    OHTableParams params;
    rep_array_unsliced<block> key;
    rep_array_unsliced<x_type> stash_xs;
    rep_array_unsliced<y_type> stash_ys;
private:
    rep_array_unsliced<block> qs_builder_order;
    rep_array_unsliced<x_type> xs_builder_order;
    rep_array_unsliced<y_type> ys_builder_order;
    rep_array_unsliced<uint> dummy_indices;
    rep_array_unsliced<x_type> xs_receiver_order;
    rep_array_unsliced<y_type> ys_receiver_order;
    block* cht_2shares;
    LocalPermutation* receiver_shuffle;

    uint query_count = 0;

    vector<bool> touched;

public:
    OHTable_array(const OHTableParams& params,
    rep_array_unsliced<x_type> xs, rep_array_unsliced<y_type> ys, 
    rep_array_unsliced<block> key)
    :params(params),
    key(key),
    stash_xs(params.stash_size),
    stash_ys(params.stash_size),
    qs_builder_order(params.total_size()),
    xs_builder_order(params.total_size()),
    ys_builder_order(params.total_size()),
    dummy_indices(params.num_dummies),
    xs_receiver_order(params.total_size()),
    ys_receiver_order(params.total_size()),
    touched(params.total_size())
    {
        assert(xs.length_Ts() == params.num_elements);
        assert(ys.length_Ts() == params.num_elements);
        assert(key.length_Ts() == prf_key_size_blocks());

        params.validate();
        
        build(xs, ys);
    }

    ~OHTable_array() {
        key.destroy();
        qs_builder_order.destroy();
        xs_builder_order.destroy();
        ys_builder_order.destroy();
        dummy_indices.destroy();
        xs_receiver_order.destroy();
        ys_receiver_order.destroy();
    }

    void build(rep_array_unsliced<x_type> xs, rep_array_unsliced<y_type> ys) {
        using namespace thread_unsafe;
        // compute qs
        uint prf_input_size_blocks = prf_key_size_blocks() + 1;
        rep_array_unsliced<block> keys_and_inputs(prf_input_size_blocks * params.num_elements);
        for (uint i = 0; i < params.num_elements; i++) {
            keys_and_inputs.copy_Ts_from(key, prf_key_size_blocks(), 0, prf_input_size_blocks * i);
            keys_and_inputs.copy_one<x_type>(prf_input_size_blocks * (i+1) - 1, xs, i);
        }
        // xs.debug_print("xs before shuffle", 4);
        // keys_and_inputs.debug_print("keys_and_inputs", 44);
        auto start = clock_start();
        prf_circuit->compute_multithreaded(qs_builder_order, keys_and_inputs, params.num_elements);
        time_total_build_prf += time_from(start);
        //qs_builder_order.debug_print("qs before shuffle", 4);
        /* quick spot check
        block* aes_results_mpc = new block[params.total_size()];
        qs_builder_order.reveal_to(1, (uint8_t*)aes_results_mpc);
        dbg_args(aes_results_mpc[999]);
        dbg_args(aes_results_mpc[1000]);*/
        ArrayShuffler builder_shuffler(params.total_size());
        xs_builder_order.copy_bytes_from(xs, xs.length_bytes);
        ys_builder_order.copy_bytes_from(ys, ys.length_bytes);
        rep_array_unsliced<uint> indices_builder_order(params.total_size());
        builder_shuffler.forward(qs_builder_order);
        builder_shuffler.forward(xs_builder_order);
        builder_shuffler.forward(ys_builder_order);
        builder_shuffler.indices(indices_builder_order);
        dummy_indices.copy_Ts_from(indices_builder_order, params.num_dummies, params.num_elements);
        //dummy_indices.debug_print("dummies", 10);
        // another quick spot check: xs and indices consistent
        /*
        uint *indices_shuffled_spot_check = new uint[params.total_size()];
        uint *xs_shuffled_spot_check = new uint[params.total_size()];
        xs_builder_order.reveal_to(1, xs_shuffled_spot_check);
        indices_builder_order.reveal_to(1, indices_shuffled_spot_check);
        for (uint i = 0; i < 10; i++) {
            // this should print x[0] through x[9] in the second column
            cerr << indices_shuffled_spot_check[i] << ' ' << xs_shuffled_spot_check[indices_shuffled_spot_check[i]] << '\n';
        }
        */
        //* in the rep_share version of build, we append index tags to qs first
        //* so here we append in the clear afterwards
        vector<block> qs_in_clear_compacted(params.num_elements);
        {
            vector<block> qs_in_clear(params.total_size());
            qs_builder_order.reveal_to(params.builder, qs_in_clear.data());

            if (party == params.builder) {
                uint j = 0;
                for (uint i = 0; i < params.total_size(); i++) {
                    // are 96 bits enough?
                    // collision probability ~2^-48 which is better than the CHT build failure we're running - or is it?
                    // have to consider lifetime of the DORAM; CHT must succeed every time
                    // we could always just store more bits
                    if (blocksEqual(qs_in_clear[i], zero_block)) continue;
                    qs_in_clear_compacted[j] = (qs_in_clear[i] & makeBlock(ULLONG_MAX, ULLONG_MAX << 32)) | makeBlock(0, i);
                    j++;
                }
                assert(j == params.num_elements);
            }
        }
        vector<block> builder_cht;
        vector<uint> stash_indices_builder(params.stash_size);
        if (party == params.builder) {
            LocalPermutation builder_local_perm(thread_unsafe::private_prg, params.num_elements);
            // this doesn't affect the stash indices, which are taken from the elements themselves
            builder_local_perm.shuffle(qs_in_clear_compacted.data());
            optimalcht::build(builder_cht, params.cht_log_single_col_len, qs_in_clear_compacted, stash_indices_builder);
        }
        /* spot check by eye: 
        // CHT elements have indices appended
        // elements in table 0 are stored at their hash0 (bits 64-64+x)
        // elements in table 1 are stored at their hash1 (bits 96-96+x)
        if (party == params.builder) {
            for (int i = 0; i < 40; i++)
                dbg_args(builder_cht[(1<<params.cht_log_single_col_len) + i]);
            vector<uint> index_count(params.total_size());
            uint num_nonzero = 0;
            // something like set equality check
            for (int i = 0; i < params.cht_full_table_length(); i++) {
                if (!blocksEqual(builder_cht[i], zero_block)) {
                    index_count[builder_cht[i][0] & ((1 << 20) - 1)]++;
                    num_nonzero++;
                }
            }
            for (int i = 0; i < params.total_size(); i++) {
                if (index_count[i] > 1) {
                    dbg("bad index", i);
                }
                assert(index_count[i] <= 1);
            }
            dbg_args(num_nonzero, params.num_elements - params.stash_size);
        }
        */
        rep_array_unsliced<block> cht_shares(params.cht_full_table_length());
        cht_2shares = new block[params.cht_full_table_length()];
        cht_shares.input(params.builder, builder_cht.data());
        cht_shares.reshare_3to2(prev_party(params.builder), next_party(params.builder), cht_2shares);

        ArrayShuffler receiver_shuffler(params.total_size());
        xs_receiver_order.copy(xs_builder_order);
        ys_receiver_order.copy(ys_builder_order);
        receiver_shuffler.forward_known_to_p_and_next(next_party(params.builder), xs_receiver_order);
        receiver_shuffler.forward_known_to_p_and_next(next_party(params.builder), ys_receiver_order);
        // copy the permutation shared between the two receivers
        if (party == prev_party(params.builder)) {
            receiver_shuffle = new LocalPermutation(receiver_shuffler.prev_shared_perm);
        } else if (party == next_party(params.builder)) {
            receiver_shuffle = new LocalPermutation(receiver_shuffler.next_shared_perm);
        }

        // compute stash indices in receiver order so that all can mark stashed elements 
        // first send out in builder order, then convert to receiver
        if (party == params.builder) {
            prev_io->send_data(stash_indices_builder.data(), params.stash_size * sizeof(uint));
            next_io->send_data(stash_indices_builder.data(), params.stash_size * sizeof(uint));
        } else if (party == next_party(params.builder)) {
            prev_io->recv_data(stash_indices_builder.data(), params.stash_size * sizeof(uint));
        } else {
            next_io->recv_data(stash_indices_builder.data(), params.stash_size * sizeof(uint));
        }

        vector<uint> stash_indices_receiver(params.stash_size);
        if (party != params.builder) {
            for (uint i = 0; i < params.stash_size; i++) {
                stash_indices_receiver[i] = receiver_shuffle->evaluate_at(stash_indices_builder[i]);
            }
        }
        
        if (party == prev_party(params.builder)) {
            next_io->send_data(stash_indices_receiver.data(), params.stash_size * sizeof(uint));
        } else if (party == params.builder) {
            prev_io->recv_data(stash_indices_receiver.data(), params.stash_size * sizeof(uint));
        }

        // pretend that stashed indices have already been queried
        // another reason it's important for stash to contain all real elements
        for (uint i = 0; i < params.stash_size; i++) {
            touched[stash_indices_receiver[i]] = true;
            stash_xs.copy_one(i, xs_receiver_order, stash_indices_receiver[i]);
            stash_ys.copy_one(i, ys_receiver_order, stash_indices_receiver[i]);
        }
        // seems to be double counting the effect of reinserting the stash_size elements
        // query_count = params.stash_size;

        // ys_builder_order.debug_print("ys_builder_order", 10);
        // ys_receiver_order.debug_print("ys_receiver_order", 10);

        keys_and_inputs.destroy();
        cht_shares.destroy();
    }

    void query (rep_array_unsliced<block> q, rep_array_unsliced<int> use_dummy, rep_array_unsliced<y_type> y, rep_array_unsliced<int> found) {
        using namespace thread_unsafe;
        assert(query_count < params.num_dummies);
        assert(q.length_Ts() == 1);
        auto start = clock_start();
        rep_array_unsliced<block> q_or_dummy(1);
        rep_array_unsliced<block> dummy(1);
        dummy.fill_random();
        auto start_ite = clock_start();
        rep_exec->if_then_else(use_dummy, dummy, q, q_or_dummy);
        time_ite = time_from(start_ite);

        block q_clear;
        // this is the correct order, other way is ~1 round slower
        q_or_dummy.reveal_to(prev_party(params.builder), &q_clear);
        q_or_dummy.reveal_to(next_party(params.builder), &q_clear);
        if (party != params.builder) {
            assert(!blocksEqual(q_clear, zero_block));
        }

        rep_array_unsliced<uint> dummy_index(1);
        dummy_index.copy_one(0, dummy_indices, query_count);
        time_before_cht = time_from(start);
        start = clock_start();
        uint index_builder_order = optimalcht::lookup_from_2shares(cht_2shares, q_clear, params.cht_log_single_col_len, 
            dummy_index, found, params.builder);
        time_in_cht = time_from(start);

        start = clock_start();
        uint index_receiver_order;
        if (party != params.builder) {
            index_receiver_order = receiver_shuffle->evaluate_at(index_builder_order);
        }

        // waiting to receive these 4 bytes costs P1 20 mics

        if (party == prev_party(params.builder)) {
            next_io->send_data(&index_receiver_order, sizeof(uint));
        } else if (party == params.builder) {
            prev_io->recv_data(&index_receiver_order, sizeof(uint));
        }

        assert(!touched[index_receiver_order]);
        touched[index_receiver_order] = true;

        y.copy_one(0, ys_receiver_order, index_receiver_order);
        query_count++;
        time_after_cht = time_from(start);
    }

    void extract(rep_array_unsliced<x_type> extract_xs, rep_array_unsliced<y_type> extract_ys) {
        assert(query_count == params.num_dummies);
        uint num_extracted = 0;
        for (uint i = 0; i < params.total_size(); i++) {
            if (touched[i]) continue;
            extract_xs.copy_one(num_extracted, xs_receiver_order, i);
            extract_ys.copy_one(num_extracted, ys_receiver_order, i);
            num_extracted++;
        }
        assert(num_extracted == params.num_elements - params.stash_size);
    }
};
}
