#!/usr/bin/env python3

import argparse
import subprocess

# latency_in_ms, bandwidth_in_Gbit_per_sec, log_N, num_levels, log_amp_factor = experiment
experiments = {
   'Figure6a': [
      [.1, 1,  20, 3, 5],
    [.6, 1, 20, 3, 5],
    [3, 1, 20, 3, 5],
    [9, 1, 20, 3, 5],
    [18, 1, 20, 3, 5],
    [40, 1, 20, 3, 5],

    [.1, 1, 25, 3, 5],
    [.6, 1, 25, 3, 5],
    [3, 1, 25, 3, 5],
    [9, 1, 25, 3, 5],
    [18, 1, 25, 3, 5],
    [40, 1,  25, 3, 5], 
   ],
   'Figure6b': [
      [8, 0.025 ,20, 3, 7],
    [8, .05, 20, 3, 7],
    [8, .25, 20, 3, 7],
    [8, .5, 20, 3, 7],
    [8, 1, 20, 3, 7],
    [8, 1.5, 20, 3, 7] 
   ],
}

parser = argparse.ArgumentParser()
parser.add_argument("experiment", help="One of " + ', '.join(experiments.keys()))
parser.add_argument("--queries", help="Num queries to run (default 5000)")
parser.add_argument("--threads", help="Num threads to use per party (default 20; this should be less than 1/3 * num cpu cores)")
args = parser.parse_args()

if args.experiment not in experiments:
    print("Unrecognized experiment:", args.experiment)
    exit(1)

if args.queries is None:
    num_queries = 5000
else:
    num_queries = args.queries

if args.threads is None:
    num_threads = 20
else:
    num_threads = args.threads

for experiment in experiments[args.experiment]:
    #print(experiment, type(experiment))
    # unpack experiment
    latency_in_ms, bandwidth_in_Gbit_per_sec, log_N, num_levels, log_amp_factor = experiment

    command = f"./benchmark_doram_locally.sh {latency_in_ms}ms {bandwidth_in_Gbit_per_sec}Gbit --num-query-tests {num_queries} --log-address-space {log_N} --num-levels {num_levels} --num-threads {num_threads} --log-amp-factor {log_amp_factor} --prf-circuit-filename LowMC_reuse_wires.txt --build-bottom-level-at-startup {'true' if log_N<20 else 'false'}"
    print(command, '\n')

    subprocess.run(command, capture_output=False, shell=True)
