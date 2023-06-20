#define DORAM_DEBUG

#include <iostream>
#include <map>
#include <unistd.h> // for nonblocking
#include <math.h>

#include "doram/bristol_fashion_array.h"
#include "doram/debug.h"
#include "doram/doram_array.h"
#include "doram/globals.h" //player's global pessions
// already included through DORAM: #include "doram/sh_rep_bin.h" //needed for circ_exec type

#include "emp-tool/emp-tool.h"
#include "emp-tool/utils/block.h" //inputs to AES are blocks

using namespace std;
using namespace emp;

// DORAM params

void initialize_resource_set(int resource_id, string prev_host, string next_host, int prev_port, int next_port)
{
    dbg_args(resource_id, prev_port, next_port);
    if (party == 1)
    {
        next_ios[resource_id] = new RepNetIO(nullptr, next_port, true);
        prev_ios[resource_id] = new RepNetIO(nullptr, prev_port, true);
    }
    else if (party == 2)
    {
        prev_ios[resource_id] = new RepNetIO(prev_host.c_str(), prev_port, true);
        next_ios[resource_id] = new RepNetIO(nullptr, next_port, true);
    }
    else
    {
        next_ios[resource_id] = new RepNetIO(next_host.c_str(), next_port, true);
        prev_ios[resource_id] = new RepNetIO(prev_host.c_str(), prev_port, true);
    }

    next_prgs[resource_id] = new PRG();
    next_ios[resource_id]->send_block(&next_prgs[resource_id]->key, 1);

    next_ios[resource_id]->flush();

    block prev_prg_key; // hi
    prev_ios[resource_id]->recv_block(&prev_prg_key, 1);

    prev_prgs[resource_id] = new PRG(&prev_prg_key);
    private_prgs[resource_id] = new PRG();
    shared_prgs[resource_id] = new PRG(&all_one_block);

    rep_execs[resource_id] = new SHRepArray(resource_id);
}

void initialize_all_resources(string prev_host, string next_host, int prev_port, int next_port) 
{
    prev_prgs = new PRG* [NUM_THREADS];
    next_prgs = new PRG* [NUM_THREADS];
    private_prgs = new PRG* [NUM_THREADS];
    shared_prgs = new PRG* [NUM_THREADS];

    prev_ios = new RepNetIO* [NUM_THREADS];
    next_ios = new RepNetIO* [NUM_THREADS];
    rep_execs = new SHRepArray* [NUM_THREADS];

    for (uint resource_id = 0; resource_id < NUM_THREADS; resource_id++)
    {
        initialize_resource_set(resource_id, prev_host, next_host, prev_port + resource_id, next_port + resource_id);
    }

    {
        using namespace thread_unsafe;
        prev_prg = prev_prgs[0];
        next_prg = next_prgs[0];
        private_prg = private_prgs[0];
        shared_prg = shared_prgs[0];
        prev_io = prev_ios[0];
        next_io = next_ios[0];
        rep_exec = rep_execs[0];
    }
}

const string UNNAMED_ARGS = "./doram.exe PARTY PREV_HOST_AND_PORT NEXT_HOST_AND_PORT CIRCUITS_DIR";

int main(int argc, char *argv[])
{
    // todo: can run again, where y is a deterministic function of x for better testing

    if (argc < 5)
    {
        cerr << UNNAMED_ARGS << "\n";
        exit(1);
    }
    // this is a global because of the inclusion of debug

    party = atoi(argv[1]);
    string prev_host_and_port(argv[2]);
    string next_host_and_port(argv[3]);
    string circuits_dir(argv[4]);
    
 //! Named Parameters!
    uint NUM_QUERY_TESTS = -1;
    bool found_num_query_tests = false;
    uint LOG_ADDRESS_SPACE = -1;
    bool found_log_address_space = false;
    uint NUM_LEVELS = -1;
    bool found_num_levels = false;
    uint LOG_AMP_FACTOR = -1;
    bool found_log_amp_factor = false;
    string PRF_CIRCUIT_FILENAME;
    bool found_prf_circuit_filename = false;
    bool BUILD_BOTTOM_LEVEL_AT_STARTUP = false;
    bool found_build_bottom_level_at_startup = false;
    bool found_num_threads = false;

    for (int i = 5; i + 1 < argc; i+=2) {
        string flag(argv[i]);
        string arg(argv[i+1]);
        if (flag == "--prf-circuit-filename") {
            PRF_CIRCUIT_FILENAME = arg;
            found_prf_circuit_filename = true;
        } else if (flag == "--build-bottom-level-at-startup") {
            if (arg == "true") {
                BUILD_BOTTOM_LEVEL_AT_STARTUP = true;
            } else if (arg == "false") {
                BUILD_BOTTOM_LEVEL_AT_STARTUP = false;
            } else {
                cout << "--build-bottom-level-at-startup: argument not recognized (accepts values {}'true', 'false'}): " << arg << '\n';
                exit(1);
            }
            found_build_bottom_level_at_startup = true;
        } else {
            uint arg_value = stoi(arg);
            if (flag == "--num-query-tests") {
                NUM_QUERY_TESTS = arg_value;
                found_num_query_tests = true;
            } else if (flag == "--log-address-space") {
                LOG_ADDRESS_SPACE = arg_value;
                found_log_address_space = true;
            } else if (flag == "--num-levels") {
                NUM_LEVELS = arg_value;
                found_num_levels = true;
            } else if (flag == "--log-amp-factor") {
                LOG_AMP_FACTOR = arg_value;
                found_log_amp_factor = true;
            } else if (flag == "--num-threads") {
                NUM_THREADS = arg_value;
                found_num_threads = true;
            } 
        }
    }

    if (!found_num_query_tests) {
        cout << "Missing argument: --num-query-tests\n";
        exit(1);
    } else if (!found_log_address_space) {
        cout << "Missing argument: --log-address-space\n";
        exit(1);
    } else if (!found_num_levels) {
        cout << "Missing argument: --num-levels\n";
        exit(1);
    } else if (!found_log_amp_factor) {
        cout << "Missing argument: --log-amp-factor\n";
        exit(1);
    } else if (!found_prf_circuit_filename) {
        cout << "Missing argument: --prf-circuit-filename\n";
        exit(1);
    } else if (!found_build_bottom_level_at_startup){
        cout << "Missing argument: --build-bottom-level-at-startup\n";
        exit(1);
    } else if (!found_num_threads){
        cout << "Missing argument: --num-threads\n";
        exit(1);
    }

    uint N = 1 << LOG_ADDRESS_SPACE;
    string prf_circ_filename = circuits_dir + "/" + PRF_CIRCUIT_FILENAME;
    string cht_circ_filename = circuits_dir + "/cht_lookup.txt";
    string stupid_level_circ_filename = circuits_dir + "/xy_if_xs_equal.txt";
    string compare_swap_circuit_filename = circuits_dir + "/compare_swap.txt";
    string dummy_check_dir = circuits_dir + "/dummy_check";
    string replace_if_dummy_dir = circuits_dir + "/replace_if_dummy";

    prf_circuit =
        new BristolFashion_array(prf_circ_filename); // make in main since we will use both for query and build
    cht_lookup_circuit_file = new BristolFashion_array(cht_circ_filename);
    xy_if_xs_equal_circuit = new BristolFashion_array(stupid_level_circ_filename);
    compare_swap_circuit_file = new BristolFashion_array(compare_swap_circuit_filename);
    for (uint log_N = 6; log_N <= 31; log_N++)
    {
        dummy_check_circuit_file[log_N] = new BristolFashion_array(dummy_check_dir + "/" + to_string(log_N) + ".txt");
        replace_if_dummy_circuit_file[log_N] =
            new BristolFashion_array(replace_if_dummy_dir + "/" + to_string(log_N) + ".txt");
    }

    string prev_host, next_host;
    uint prev_port = 0, next_port = 0;
    parse_host_and_port(prev_host_and_port, prev_host, prev_port);
    parse_host_and_port(next_host_and_port, next_host, next_port);

    initialize_all_resources(prev_host, next_host, prev_port, next_port);

    using namespace thread_unsafe;
    optimalcht::lookup_circuit = new sh_riro::Circuit(*cht_lookup_circuit_file);

    init_timing_file();

    dbg("got to input setup");
    dbg_args(prf_key_size_blocks());

    //* setup inputs, in this case a list [(X, Y) to secret share], currently using 0,...,N-1, both are 64 bit values
    //! remember, N is also reserved!
    rep_array_unsliced<y_type> *ys = nullptr;
    if (BUILD_BOTTOM_LEVEL_AT_STARTUP)
    {
        ys = new rep_array_unsliced<y_type>(N - 1);
        {
            vector<y_type> _ys_clear(N - 1);
            for (uint i = 0; i < N - 1; i++)
            {
                _ys_clear[i] = i + 1;
            }
            ys->input_public(_ys_clear.data());
        }
    }
    map<x_type, y_type> simulated_doram;
    if (BUILD_BOTTOM_LEVEL_AT_STARTUP)
    {
        for (uint i = 1; i < N; i++)
        {
            simulated_doram[i] = i;
        }
    }
    //* common setup ends -- do actual functionality testsing
    dbg("got to building DORAM");

    auto _start = clock_start();
    DORAM doram(LOG_ADDRESS_SPACE, ys, NUM_LEVELS, LOG_AMP_FACTOR);
    time_doram_constructor = time_from(_start);

    // can optimize compute for mem, but ig I'm not gonna do it
    // IT IS SO MUCH FUN TO WRITE THIS TEST! IT IS COMPARATIVELY SO CLEAN OMG I WANT TO SMILE AND CRY
    // could have done this with less mem, but went for clearness instead
    assert(LOG_ADDRESS_SPACE < sizeof(y_type) * 8 - doram.get_num_levels() &&
           "make sure that the test values we're using for ys don't get clobbered by alibi bits");

    //*
    vector<x_type> locs_queried;
    //*query doram
    for (uint i = 0; i < NUM_QUERY_TESTS; i++)
    {
        // setup qry
        int is_write;
        shared_prg->random_data(&is_write, 1);
        is_write &= 1;

        rep_array_unsliced<int> is_write_rep(1);
        is_write_rep.input_public(&is_write);

        x_type x_qry;
        y_type y_qry;
        x_qry = sample_unif_from_prg(shared_prg, 1, N);
        y_qry = sample_unif_from_prg(shared_prg, 1, N) * is_write;

        rep_array_unsliced<x_type> x_qry_rep(1);
        rep_array_unsliced<y_type> y_qry_rep(1);

        x_qry_rep.input_public(&x_qry);
        y_qry_rep.input_public(&y_qry);

        locs_queried.push_back(x_qry);
        // exec qry localy

        rep_array_unsliced<y_type> qry_res_rep = doram.read_and_write(x_qry_rep, y_qry_rep, is_write_rep);
        y_type qry_res;
        qry_res_rep.reveal_to_all(&qry_res);

        if (party == 1)
        {
            // dbg("query", x_qry, simulated_doram[x_qry], qry_res, y_qry);
            // if (qry_res != simulated_doram[x_qry]) {
            // dbg("got qry_res", qry_res, "and expected ", simulated_doram[x_qry]);
            // dbg(i, ": ", (is_write ? "write" : "read"), " to ", x_qry, " of ", y_qry); //!uncomment these if failing
            // }

            assert(qry_res == simulated_doram[x_qry]);
            // dbg(i, "-SUCCESS: ", (is_write ? "write" : "read"), " to ", x_qry, " of ", y_qry);
        }
        if (is_write)
        {
            simulated_doram[x_qry] = y_qry;
        }
    }
    dbg("completed ", NUM_QUERY_TESTS, " succesfully queries -- hurray!");

    //? a cool thing to do is append all the "settings" that passed to a file
    /*

        dbg("\n\n\nFinished making reads and writes let's revisit our queries\n\n\n");

        for (x_type x_qry : locs_queried) // check all teh locations we wrote to
        {
            rep_array_unsliced<x_type> x_qry_rep(1);
            x_qry_rep.input_public(&x_qry);
            // read only, not write
            rep_array_unsliced<y_type> y_qry_rep(1);
            rep_array_unsliced<int> is_write(1);

            rep_array_unsliced<y_type> qry_res_rep = doram.read_and_write(
                x_qry_rep, y_qry_rep, is_write);

            y_type qry_res;
            qry_res_rep.reveal_to_all(&qry_res);

            assert(qry_res == simulated_doram[x_qry]);
            // dbg("-SUCCESS: ", "read to ", x_qry);
        }
    */
    time_total = time_from(_start);

    dbg_args(time_total_build_prf);

    // *check that doram compiles

    // dbg("new statement!");
    // dbg(" doram tests ", "PASSED");
    cout << "Success!\n"; // if we got here, nothing failed

    double total_num_bytes_on_all_threads = 0; 
    for (uint i = 0; i < NUM_THREADS; i++)
    {
        total_num_bytes_on_all_threads += prev_ios[i]->schannel->counter;
        total_num_bytes_on_all_threads += prev_ios[i]->rchannel->counter;
        total_num_bytes_on_all_threads += next_ios[i]->schannel->counter;
        total_num_bytes_on_all_threads += next_ios[i]->rchannel->counter;
    }
    

    // PRINT TIMES
    timing_file << "DORAM Parameters\n"
                << "Number of queries: " << NUM_QUERY_TESTS << '\n'
                << "Build bottom level at startup: " << BUILD_BOTTOM_LEVEL_AT_STARTUP << '\n'
                << "Log address space size: " << LOG_ADDRESS_SPACE << '\n'
                << "Data block size (bits): " << 8 * sizeof(y_type) << '\n'
                << "Log linear level size: " << (doram.log_sls) << '\n'
                << "Log amp factor: " << LOG_AMP_FACTOR << '\n'
                << "Num levels: " << doram.num_levels << '\n'
                << "PRF circuit file: " << PRF_CIRCUIT_FILENAME<< "\n"
                << "Num threads: " << NUM_THREADS << '\n'
                << '\n'
                << "Timing Breakdown\n"
                << "Total time including builds: " << time_total << " us\n"
                << "Time spent in queries: " << time_total_queries << " us\n"
                << "Time spent in query PRF eval: " << time_total_query_prf << " us\n"
                << "Time spent querying linear level: " << time_total_query_stupid << " us\n"
                << "Time spent in build PRF eval: " << time_total_build_prf << " us\n"
                << "Time spent in batcher sorting: " << time_total_batcher << " us\n"
                << "Time spent building bottom level: " << time_total_builds.back() << " us\n"
                << "Time spent building other levels: "
                << accumulate(time_total_builds.begin(), time_total_builds.end() - 1, 0) << " us \n"
                << '\n'
                << "SUMMARY\n"
                << "Total time including builds: " << time_total << " us \n"
                << "Total number of bytes sent: "
                << total_num_bytes_on_all_threads<< "\n"
                << "Queries/sec: "
                << (NUM_QUERY_TESTS / (time_total)* pow(10, 6)) 
                << '\n';
    // CLOSE DESCRIPTORS
    timing_file.close();
    cout << "Output written to doram_timing_report" << party << ".txt\n"; 
    return 0;
}
