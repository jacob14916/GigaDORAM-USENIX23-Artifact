#pragma once

#include "emp-tool/emp-tool.h"
#include "rep_net_io_channel.h"
#include <thread>
#include <map> 


using namespace std;

namespace emp
{

class AbandonIO : public IOChannel<AbandonIO>
{
  public:
    void flush()
    {
    }

    void send_data_internal(const void *data, int len)
    {
    }

    void recv_data_internal(void *data, int len)
    {
    }
};

class BristolFashion_array;

// party conventions:
// 1: Party 1
// 2: Party 2
// 3: Party 3
int party = 0; 

uint NUM_THREADS = 2;

// Each of the thread_unsafe:: resources is an array of length num_threads
// which has to be initialized in main. The 0th element is the default
// for code that will never run in a thread. Code that runs in threads
// should look up its thread's assigned resources.
// The ith thread_unsafe::prev resource is assumed to be connected to
// the previous party's ith thread_unsafe::next resource.

class SHRepArray;
class MalRepArray;

PRG **prev_prgs;
PRG **next_prgs;
PRG **private_prgs;
PRG **shared_prgs;

RepNetIO **prev_ios;
RepNetIO **next_ios;
SHRepArray **rep_execs;


namespace thread_unsafe {

PRG *prev_prg;
PRG *next_prg;
// does private_prg really need to be thread_unsafe?
PRG *private_prg;
PRG *shared_prg;

RepNetIO *prev_io;
RepNetIO *next_io;
SHRepArray *rep_exec;

}

// TIMING
fstream timing_file;
fstream special_debug_file;

// ignore setup
double time_total = 0;         
vector<double> time_total_builds;
double time_total_build_prf = 0;
double time_total_batcher = 0;
double time_total_deletes = 0; 
double time_total_queries = 0;
double time_total_query_prf = 0;
double time_total_query_stupid = 0;
double time_total_shuffles = 0;
double time_total_cht_build = 0;
double time_total_transpose = 0;

// The following timings are not thread safe and commented out for now
/*
double time_total_mand = 0;
double time_compute_start = 0;
double time_compute_finish = 0;
double time_main_loop = 0;
*/

// double time_total_network = 0; // defined in rep_net_io to avoid include problem, can still be accessed globaly

double time_doram_constructor = 0; //? what is this for?

typedef unsigned long long ull;

typedef uint32_t x_type;
typedef uint64_t y_type;

BristolFashion_array *xy_if_xs_equal_circuit = nullptr;
BristolFashion_array *cht_lookup_circuit_file = nullptr;
BristolFashion_array *prf_circuit = nullptr;
BristolFashion_array *replace_if_dummy_circuit_file[32];
BristolFashion_array *dummy_check_circuit_file[32];
BristolFashion_array *compare_swap_circuit_file = nullptr;

} // namespace emp
