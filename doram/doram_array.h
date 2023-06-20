#pragma once

 // typedef ull when we use it for y_type
#include "debug.h"
#include "globals.h" //player's global prgs&ios
#include "ohtable_array.h"
#include "stupid_level.h"
#include "batcher.h"

#include <fstream>     //log
#include <sys/types.h> // uint

//#define q_type block;

// Hack for testing: made these editable
// TODO: proper DORAM config files
uint STASH_SIZE_USE_EMPIRICAL_CHT_BOUNDS = 8;
uint STASH_SIZE_USE_PROVEN_CHT_BOUNDS = 50;

const bool use_proven_cht_bounds = false;

namespace emp
{
class DORAM
{
  public:
// constructor inits these
    uint log_address_space_size; 
    uint num_levels; 
    uint log_amp_factor;
    rep_array_unsliced<block> prf_keys;

    uint log_sls, stupid_fill_time, stash_size; // params set by testing 
    uint amp_factor;

    double d; // ratio of CHT_col_len/actual_data_len

    // prf stuff

    //* ohtable levels are 0-indexed: 
    //* first level after stupid level is 0
    //* bottom level is num_levels - 1
    vector<OHTable_array*> ohtables;
    StupidLevel *stupid_level; 

    //?all the other ohtable params: make a struct?
    vector<uint> base_b_state_vec;

    // to log
    fstream logger;

    bool had_initial_bottom_level;
public:
    DORAM(uint log_address_space_size, rep_array_unsliced<y_type> *ys_no_dummy_room, uint num_levels, uint log_amp_factor)
        : 
        log_address_space_size(log_address_space_size),
        num_levels(num_levels),
        log_amp_factor(log_amp_factor),
        prf_keys(num_levels * prf_key_size_blocks())
    {
        // input should be 1 shorter than address space size
        if (ys_no_dummy_room == nullptr) {
            had_initial_bottom_level = false;
        } else {
            had_initial_bottom_level = true;
            assert((1U << log_address_space_size) - 1 == ys_no_dummy_room->length_Ts());
        }

        dbg("Initializing DORAM");
        init_logger();
        decide_params(); //* ALWAYS DO THIS FIRST ALL OTHER STUFF DEPENDS ON THESE INITIALIZATIONS
        assert(num_elements_at(num_levels - 1, 1) ==
               (uint)(1 << log_address_space_size) -
                   1); // sanity check on decide params. -1 becasue we don't include 0 or 2^N
        dbg("initialized DORAM params");

        //*init stupid
        stupid_level = new StupidLevel(1 << log_sls);

        //*sanity checks
        assert("must have at least 1 level (+ stupid level which is automatically included) in the DORAM!" &&
               num_levels >= 1);
        assert("must reserve N through 2N-1 as dummy locations" && log_address_space_size + 1 <= sizeof(x_type) * 8);
        // assert("test crypto version first" && use_proven_cht_bounds);

        //* doram data-specific init

        ohtables.resize(num_levels);

        if (had_initial_bottom_level) {
            //* make xs for largest level
            //*we put in 2^N-1 ys list with no room for num dummies. We move them to an array with room for num dummies
            uint bottom_level_els = num_elements_at(num_levels - 1);

            rep_array_unsliced<x_type> xs_no_dummy_room(bottom_level_els);
            vector<x_type> xs_1_to_n_clear(bottom_level_els);
            for (uint i = 0; i < bottom_level_els; i++){
                xs_1_to_n_clear[i] = i+1;
            }
            assert(xs_1_to_n_clear[0] == 1);
            assert(xs_1_to_n_clear.size() == xs_no_dummy_room.length_Ts());
            xs_no_dummy_room.input_public(xs_1_to_n_clear.data());

            dbg("creating largest level and inserting stash");
            new_ohtable_of_level(num_levels - 1, xs_no_dummy_room, *ys_no_dummy_room); // data_len is known using the base
            insert_stash(num_levels - 1); // this will ussuly happen as part of rebuild (the only other occusion where
        } else {
            // fake having a stash
            stupid_level->skip(stash_size);
        }
                                      // we build a level) but now we have to do it manually
        //
        //* call tests
        __all_tests_have_passed = true; //__test_rebuild_w_reinsert_driver(); //(this test is outide )

        //! I didn't deallocate ohtable after rebuild testing!
        dbg("Done building DORAM");
    }

private:

    void decide_params()
    {
        d = use_proven_cht_bounds ? 2 : 1.2;

        amp_factor = 1 << log_amp_factor;

        log_sls = log_address_space_size - (num_levels - 1) * log_amp_factor;
        //! for small input sizes only concern (can't really run large inputs now)
        stash_size = use_proven_cht_bounds ? STASH_SIZE_USE_PROVEN_CHT_BOUNDS : STASH_SIZE_USE_EMPIRICAL_CHT_BOUNDS;
        assert(stash_size < (1U << log_sls) && "stash can't be smaller than stupid level");
         
        stupid_fill_time = (1U << log_sls) - stash_size;
        
        base_b_state_vec.resize(num_levels);

        if (time_total_builds.size() < num_levels) {
            time_total_builds.resize(num_levels);
        }
    }


    void init_logger()
    {
        logger.open("doram_build_log.txt", ios::out);
        if (!logger)
        {
            cerr << "failed to open DORAM logger, exiting..." << endl;
            exit(1);
        }
        logger << "keep in mind: only p1 will write to logger\n";
    }

    void logger_write(string msg)
    {
        if (party == 1)
        {
            // dbg("in logger write", msg);
            logger << msg << endl;
        }
    }

    uint get_num_alive_levels()
    {
        uint cnt = 0;
        for (uint i = 0; i < num_levels; i++)
        {
            cnt += ohtables[i] != nullptr;
        }
        return cnt;
    }

    //*xs and ys must have len data_len+get_num_dummies(data_len)
    // base b should be incremmented on the outside
    void new_ohtable_of_level(uint level_num, rep_array_unsliced<x_type> xs, rep_array_unsliced<y_type> ys)
    {
        auto _start = clock_start();

        assert(level_num < num_levels);
        if (level_num == num_levels - 1)
        {
            base_b_state_vec[level_num] = 1;
        }
        else
        {
            base_b_state_vec[level_num] += 1;
        } 


        uint state = base_b_state_vec[level_num];
        assert(state < (uint)(1 << log_amp_factor));

        dbg("building level " + to_string(level_num) + " with state " + to_string(state));
        logger_write("building level " + to_string(level_num) + " with state " + to_string(state));

        OHTableParams params;
        params.num_elements = num_elements_at(level_num);
        params.num_dummies = get_num_dummies(level_num);
        params.stash_size = stash_size;
        params.builder = 1;
        params.cht_log_single_col_len = get_log_col_len(level_num);
        rep_array_unsliced<block> key = generate_prf_key(level_num);

        OHTable_array* new_ohtable = new OHTable_array(params, xs, ys, key);

        time_total_builds[level_num] += time_from(_start);

        ohtables[level_num] = new_ohtable;
    }

    void delete_ohtable(uint lvl)
    {
        auto _start = clock_start();
        assert(ohtables[lvl] != nullptr);
        delete ohtables[lvl];
        ohtables[lvl] = nullptr;
        if (lvl < num_levels - 1)
        {
            if (base_b_state_vec[lvl] == amp_factor - 1) {
                base_b_state_vec[lvl] = 0;
            } else {
                // do nothing because base_b_state_vec[lvl] will be incremented in new_ohtable_of_level
            }
        }
        time_total_deletes += time_from(_start);
    }

    rep_array_unsliced<block> generate_prf_key(uint level_num) {
        rep_array_unsliced<block> key(prf_key_size_blocks());
        // TODO: replace with LowMC key generation procedure
        key.fill_random();
        prf_keys.copy_Ts_from(key, prf_key_size_blocks(), 0, level_num * prf_key_size_blocks());
        return key;
    }

    
    // we err on the larget side of things, hence the +1 for possible round-downs
    uint get_log_col_len(uint level_num, uint _state = UINT_MAX)
    {
        assert(level_num < num_levels &&
               (_state == UINT_MAX ||
                _state < (uint)(1 << log_amp_factor))); // catch obvious confusion bugs, probably will help

        uint state = (_state == UINT_MAX ? (level_num == num_levels - 1 ? 1 : base_b_state_vec[level_num]) : _state);
        uint base_b_num = level_num == num_levels - 1 ? 1 : state;
        return level_num * log_amp_factor + log_sls + 31 - __builtin_clz(d * base_b_num) +
               __builtin_popcount((d * base_b_num) != 1);
    }

    uint get_num_dummies(uint level_num) // B^i * stupid_level_size (not including stash size )
    {
        assert(level_num < num_levels); // catch obvious confusion bugs, probably will help with refactoring
        return (1 << (log_amp_factor * level_num)) * (stupid_fill_time); 
    }

    uint num_elements_at(uint level_num, uint state_override = UINT_MAX) //*this depends on the state vector
    {
        assert(level_num < num_levels); 

        if (level_num == num_levels - 1)
        {
            assert(state_override == 1 || state_override == UINT_MAX);
            //-1 because 2^N and 0 are not valid addresses
            return (1 << (log_amp_factor * level_num)) * (1 << log_sls) - 1; 
        }

        uint state = (state_override == UINT_MAX ? base_b_state_vec[level_num] : state_override);
        return (1 << (log_amp_factor * level_num)) * state * (1 << log_sls);
    }

    uint total_num_els_and_dummies(uint level_num, uint state_override = UINT_MAX) {
        return num_elements_at(level_num, state_override) + get_num_dummies(level_num);
    }

  public:
    uint get_num_levels()
    {
        return num_levels;
    } // we need this for alibi testing

    bool __all_tests_have_passed = false;
    // for now do a soft initialize which calls another function, perhaps we would want to do an
    // incremental intialize
    //? note that xs and ys need be deallocated by caller? (that's the way it is, is it a good practice?, it's
    // different
    // than @ ohtable)
    ~DORAM()
    {
        auto _start = clock_start();
        dbg("destructing DORAM");
        delete stupid_level; // stupid level is allocated in constructor and never deallocated (exccept from here)

        for (uint i = 0; i < num_levels; i++)
        {
            if (ohtables[i] != nullptr)
            {
                delete_ohtable(i);
            }
        }
        logger << "destructed DORAM succesfully\n";
        logger.close();
        dbg("destructed succesfully");
        time_total_deletes += time_from(_start);
    }

    rep_array_unsliced<y_type> read_and_write(rep_array_unsliced<x_type> qry_x, rep_array_unsliced<y_type> qry_y, 
        rep_array_unsliced<int> is_write)
    {
        using namespace thread_unsafe;
        assert(is_write.length_bytes > 0);

        //*set up global params
        // todo can reuse this mem

        // Four accumulator variables
        // Alibi mask accumulator
        rep_array_unsliced<int> alibi_mask(num_levels); // reset alibi mask
        rep_array_unsliced<y_type> y_accum(1);
        rep_array_unsliced<int> found(1);
        rep_array_unsliced<int> use_dummy(1);

        //* check if a rebuild is necessery
        if (!stupid_level->is_writeable()) // if we need to rebuild before we can query
        {
            rebuild(); // always must triggered from stupid -- there is no way stupid is queriable
            // dbg("\n\n\n\n FINISHED REBUILD OF (SOME) LEVEL \n\n\n\n");
            //  if we are querying from the fake query driver, we don't want to run real queries and better exit
        }

        auto _start = clock_start(); //* we start here because before we do very minimal work and we don't wanna double
                                     // count the builds

        //*
        //*compute PRFs on for the query on all levels
        //*
        // we do this after potential rebuild so we know what levels will be around, but actually, this can be
        // parallilzed
        // todo: haven't even built it, but there will be optimisations
        // todo all this stuff can be moved to be global to DORAM

        // prepare input setting up chunks of 128 * (keysize + 1) bits 
        // s.t first 128 * keysize are the key, 
        // then next sizeof(x_type)*8 are qry_x
        // trailing bits are 0
                
        uint prf_input_size_blocks = (prf_key_size_blocks() + 1);
        rep_array_unsliced<block> prf_input(prf_input_size_blocks * num_levels);
        for (uint i = 0; i < num_levels; i++) {
            if (ohtables[i] != nullptr) {
                prf_input.copy_Ts_from(prf_keys, prf_key_size_blocks(), prf_key_size_blocks() * i, prf_input_size_blocks * i);
                prf_input.copy_one(prf_input_size_blocks * (i + 1) - 1, qry_x);
            }
        }

        //*actually eval prf
        rep_array_unsliced<block> prf_output(num_levels);
        auto query_prf_start = clock_start();
        prf_circuit->compute(prf_output, prf_input, num_levels, rep_exec);
        time_total_query_prf += time_from(query_prf_start);


        assert(is_write.length_bytes > 0);
        //*
        //*we rebuilt if necessery, we prepered PRF evals, now query -- doing circuit logic
        //*

        //* query stupid & extract aliby bits
        auto query_stupid_start = clock_start();
        stupid_level->query(qry_x, y_accum, found);
        time_total_query_stupid += time_from(query_stupid_start);

        assert(is_write.length_bytes > 0);
        extract_alibi_bits(y_accum, alibi_mask);

        //__print_replicated_val(found, "found (after qry stupid) is");
        //*traverse the rest of the hierarchy
        for (uint i = 0; i < num_levels; i++)
        {
            if (ohtables[i] != nullptr)
            {

                use_dummy.copy_one(0, alibi_mask, i);
                use_dummy.xor_with(found);

                /*
                dbg(i);
                found.debug_print("found");
                use_dummy.debug_print("use_dummy");
                alibi_mask.debug_print("alibi_mask");
                */
                rep_array_unsliced<y_type> y_returned(1);
                rep_array_unsliced<int> found_returned(1);
                ohtables[i]->query(prf_output.window(i, 1), use_dummy, y_returned, found_returned);

                y_accum.xor_with(y_returned);
                found.xor_with(found_returned);

                extract_alibi_bits(y_accum, alibi_mask);
                //__print_replicated_val(found, "after querying level " + to_string(i) + " found is now:");
            }
        }

        assert(is_write.length_bytes > 0);

        rep_array_unsliced<y_type> write_y(1);
        rep_exec->if_then_else(is_write, qry_y, y_accum, write_y);
        // reset alibi mask for this element to 0
        write_y.apply_and_mask(get_all_ones_rightshifted_by<y_type>(num_levels));
        stupid_level->write(qry_x, write_y);

        // reset alibi mask for returned y value as well
        y_accum.apply_and_mask(get_all_ones_rightshifted_by<y_type>(num_levels));

        time_total_queries += time_from(_start);
        return y_accum;
    }

    void rebuild()
    {
        // stupid level must be full for us to rebuild, no point checking it
        uint rebuild_to = 0;
        bool need_to_extract_from_rebuild_to = false;
        x_type tot_num_to_extract = (uint)(1 << log_sls); // we at least gotta extract from the stupid level, we see
                                                          // how much we will need to extract with this variable/

        //*look for level we rebuild to
        for (; rebuild_to < num_levels - 1; rebuild_to++)
        {
            if (base_b_state_vec[rebuild_to] < amp_factor - 1) // once found a level that isn't fully built
            {
                if (ohtables[rebuild_to] != nullptr)
                {
                    assert(base_b_state_vec[rebuild_to] > 0);
                    need_to_extract_from_rebuild_to = true;
                    assert(num_elements_at(rebuild_to) == ohtables[rebuild_to]->params.num_elements);
                    tot_num_to_extract += num_elements_at(rebuild_to);
                }

                break; // we found the level to build, we will not collect elements from greater levels
            }
            else // if the level is fully built, we will extract all of it's contents (but it is not the level we
                 // rebuild to)
            {
                tot_num_to_extract += num_elements_at(rebuild_to);
            }
        } // if all levels (but the last level) are fully built, we would have rebuild_to incrememnted to
          // num_levels-1, and rebuild that level
        if (rebuild_to == num_levels - 1)
        {
            if (base_b_state_vec[rebuild_to] != 0) {
                assert(ohtables[rebuild_to] != nullptr);
                need_to_extract_from_rebuild_to = true;
                tot_num_to_extract += num_elements_at(rebuild_to);
            } else {
                // we actually can be in bottomless mode and have a bottom level
                assert(!had_initial_bottom_level);
                assert(ohtables[rebuild_to] == nullptr);
            }
        }

        dbg_args(base_b_state_vec, rebuild_to);
        // check that the number of elements to extract equals the number of elements
        // that will reside in the new ohtable
        assert((rebuild_to == num_levels - 1)
                || tot_num_to_extract == num_elements_at(rebuild_to, base_b_state_vec[rebuild_to] + 1));

        rep_array_unsliced<x_type> extracted_list_xs(tot_num_to_extract);
        rep_array_unsliced<y_type> extracted_list_ys(tot_num_to_extract);

        uint num_extracted = 0;

        //* extract_stupid

        stupid_level->extract(extracted_list_xs.window(0, (1 << log_sls)),
            extracted_list_ys.window(0, (1 << log_sls)));
        num_extracted += (uint)(1 << log_sls); // there is an assert in extract checking that we onlu extract when
                                               // we extract sls many els
        // dbg("num extracted from stupid", num_extracted);
        for (uint i = 0; i < (rebuild_to + need_to_extract_from_rebuild_to); i++)
        {
            ohtables[i]->extract(extracted_list_xs.window(num_extracted, num_elements_at(i)),
                             extracted_list_ys.window(num_extracted, num_elements_at(i)));

            num_extracted += num_elements_at(i);
        }
        assert(num_extracted == tot_num_to_extract);

        //*we now have all the xs and the ys into a list, let's re-number the dummies

        // dbg("made it right before relabel dummies for rebuild_to ", rebuild_to, " and will relabel ", num_extracted,
        //     " elements");

        if (rebuild_to == num_levels - 1) //*it is different to build to largest level
        {
            // dbg("building a largest level");
            if (had_initial_bottom_level) {
                ArrayShuffler pre_cleanse_shuffler(num_extracted);
                pre_cleanse_shuffler.forward(extracted_list_xs);
                pre_cleanse_shuffler.forward(extracted_list_ys);
            }

            rep_array_unsliced<x_type> cleansed_for_bottom_level_xs(num_elements_at(num_levels - 1));
            rep_array_unsliced<y_type> cleansed_for_bottom_level_ys(num_elements_at(num_levels - 1));

            cleanse_bottom_level(extracted_list_xs, extracted_list_ys, 
                cleansed_for_bottom_level_xs, cleansed_for_bottom_level_ys, 
                log_address_space_size);

            for (uint i = 0; i < num_levels; i++)
            {
                if (i == num_levels - 1 && !had_initial_bottom_level && base_b_state_vec[i] == 0) break;
                delete_ohtable(i);
            }
            new_ohtable_of_level(num_levels - 1, cleansed_for_bottom_level_xs, cleansed_for_bottom_level_ys);

            cleansed_for_bottom_level_xs.destroy();
            cleansed_for_bottom_level_ys.destroy();
        }
        else
        {
            assert(num_extracted == num_elements_at(rebuild_to, base_b_state_vec[rebuild_to] + 1));
            relabel_dummies(extracted_list_xs, log_address_space_size);
            // I suspect that we will find a problem here -- our DORAM may contain duplicates?
            for (uint i = 0; i < rebuild_to; i++)
            {
                delete_ohtable(i);
            }
            if (need_to_extract_from_rebuild_to)
            {
                delete_ohtable(rebuild_to);
            }
            new_ohtable_of_level(rebuild_to, extracted_list_xs, extracted_list_ys);
        }

        stupid_level->clear();

        // dbg("rebuild succesfull, reinserting stash...");

        insert_stash(rebuild_to);
        extracted_list_xs.destroy();
        extracted_list_ys.destroy();
    }

    void cleanse_bottom_level(rep_array_unsliced<x_type> extracted_list_xs, 
        rep_array_unsliced<y_type> extracted_list_ys, 
        rep_array_unsliced<x_type> cleansed_for_bottom_level_xs,
        rep_array_unsliced<y_type> cleansed_for_bottom_level_ys,
        uint log_N
        )
    {
        uint num_extracted = extracted_list_xs.length_Ts();
        rep_array_unsliced<block> cleanse_bottom_level_circuit_input(num_extracted);
        rep_array_unsliced<block> cleanse_bottom_level_circuit_output(num_extracted);
        for (uint i = 0; i < num_extracted; i++) {
            cleanse_bottom_level_circuit_input.copy_one(i, extracted_list_xs, i);
        }
        dummy_check_circuit_file[log_N]->compute_multithreaded(cleanse_bottom_level_circuit_output, cleanse_bottom_level_circuit_input, 
            num_extracted);


        if (!had_initial_bottom_level)
        {
            for (uint i = 0; i < num_extracted; i++) {
                cleanse_bottom_level_circuit_output.copy_bytes_from(extracted_list_xs, 4, 4 * i, 16 * i + 4);
                cleanse_bottom_level_circuit_output.copy_bytes_from(extracted_list_ys, 8, 8 * i, 16 * i + 8);
            }
            // cleanse_bottom_level_circuit_output.debug_print("before sort");
            batcher::sort<block>(compare_swap_circuit_file, cleanse_bottom_level_circuit_output);
            // cleanse_bottom_level_circuit_output.debug_print("after sort");
            uint num_to_extract = cleansed_for_bottom_level_xs.length_Ts();
            for (uint i = 0; i < num_to_extract; i++) {
                cleansed_for_bottom_level_xs.copy_bytes_from(cleanse_bottom_level_circuit_output, 4, 16 * i + 4, 4 * i);
                cleansed_for_bottom_level_ys.copy_bytes_from(cleanse_bottom_level_circuit_output, 8, 16 * i + 8, 8 * i);
            }
            relabel_dummies(cleansed_for_bottom_level_xs, log_N);
            // cleansed_for_bottom_level_xs.debug_print("after relabel");
        }
        else {
            // this is a 128x inefficiency in output size
            vector<block> is_dummy(num_extracted);
            cleanse_bottom_level_circuit_output.reveal_to_all(is_dummy.data());

            uint num_real_els_found = 0;
            for (uint i = 0 ; i < num_extracted ; i++) {
                if (getLSB(is_dummy[i])) continue;
                cleansed_for_bottom_level_xs.copy_one(num_real_els_found, extracted_list_xs, i);
                cleansed_for_bottom_level_ys.copy_one(num_real_els_found, extracted_list_ys, i);
                num_real_els_found++;
            }
            // every real element in the DORAM should be collected here
            assert(num_real_els_found == cleansed_for_bottom_level_xs.length_Ts());
            cleanse_bottom_level_circuit_input.destroy();
            cleanse_bottom_level_circuit_output.destroy();
        }
    }

    // overwrites extracted_list_xs
    static void relabel_dummies(rep_array_unsliced<x_type> extracted_list_xs, uint log_N) {
        uint num_extracted = extracted_list_xs.length_Ts();
        rep_array_unsliced<block> relabel_dummies_circuit_input(num_extracted);
        rep_array_unsliced<block> relabel_dummies_circuit_output(num_extracted);
        rep_array_unsliced<uint> new_dummy_label(1);
        for (uint i = 0; i < num_extracted; i++) {
            relabel_dummies_circuit_input.copy_one(i, extracted_list_xs, i);
            uint new_dummy_label_clear = (1 << log_N) + i;
            new_dummy_label.input_public(&new_dummy_label_clear);
            relabel_dummies_circuit_input.copy_bytes_from(new_dummy_label, 
                sizeof(uint), 0, i * sizeof(block) + sizeof(uint));
        }
        replace_if_dummy_circuit_file[log_N]->compute_multithreaded(relabel_dummies_circuit_output, relabel_dummies_circuit_input, num_extracted);
        for (uint i = 0; i < num_extracted; i++) {
            extracted_list_xs.copy_bytes_from(relabel_dummies_circuit_output, sizeof(uint), 
                i * sizeof(block), i * sizeof(uint));
        }
        relabel_dummies_circuit_input.destroy();
        relabel_dummies_circuit_output.destroy();
        new_dummy_label.destroy();
    }

    // runs a test on rebuilding
    //? maybe I do want some params in...
    /*
        the workings of doram are so intertwined that I think pretty much no matter what I do, if I don't basically
       run DORAM, then I wouldn't be able to test it properly. For example, here, I populated a number of levels and
       fake queried them w/o reinserting and while keeping track of what I queried, and in the end (also checking
       stashes) I made sure everything made it to the rebuild_to level. Still, w/o reinsetion, the data counts would
       be wrong for the rebuild-- eh maybe not? no I think this can still work..lets try more
    */

    void insert_stash(uint level_num)
    {
        assert((level_num < num_levels) && (ohtables[level_num] != nullptr));
        OHTable_array *cur_ohtable = ohtables[level_num];

        y_type alibi_mask = get_all_zero_except_nth_from_highest<y_type>(level_num);

        rep_array_unsliced<x_type> stash_xs = cur_ohtable->stash_xs;
        rep_array_unsliced<y_type> stash_ys = cur_ohtable->stash_ys;

        stash_ys.apply_or_mask(alibi_mask);

        // stash_xs.debug_print("stash_xs");
        // stash_ys.debug_print("stash_ys");

        stupid_level->write(stash_xs, stash_ys);
    }

    // clears teh heirchical structure and 0's the state
    void clear_doram()
    {
        for (uint i = 0; i < num_levels; i++)
        {
            if (ohtables[i] != nullptr)
            {
                delete_ohtable(i);
            }
        }
    }

    void extract_alibi_bits(rep_array_unsliced<y_type> y_accum, rep_array_unsliced<int> alibi_mask) {
        for (uint i = 0; i < num_levels; i++)
        {
            // this is not wrong because we're using y_accum (not y)
            // so won't be reset to 0 if not found at level after found
            y_accum.extract_bit_xor(sizeof(y_type) * 8 - 1 - i, alibi_mask.window(i, 1));
        }
    }

    // this is the old version of this function. Because I tried to do it with no reinserting, I was short on the
    // reinserted elements for anything more than building stupid into l0, which worked
};
} // namespace emp
