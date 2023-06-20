#!/usr/bin/env python3
import argparse
import subprocess

lowmc = "LowMC_reuse_wires.txt"
aes = "aes_128_extended.txt"

# log_N, num_levels, log_amp_factor, prf_circuit_file = experiment
experiments = {
'Figure5' : [
    [12,2,4,lowmc],
    [13,2,5,lowmc],
    [14,2,6,lowmc],
    [15,2,6,lowmc],
    [17,3,4,lowmc],
    [18,4,3,lowmc],
    [19,3,5,lowmc],
    [20,4,4,lowmc],
    [21,4,4,lowmc],
    [22,4,5,lowmc],
    [23,4,5,lowmc],
    [24,5,4,lowmc],
    [25,5,4,lowmc],
    [26,4,6,lowmc],
    [27,4,6,lowmc],
    [28,5,5,lowmc],
    [29,6,4,lowmc],
    [30,4,7,lowmc],
    [31,4,7,lowmc]
],
'Table1' : [
    [20,4,4,lowmc],
    [23,4,5,lowmc],
    [26,4,6,lowmc],
    [29,6,4,lowmc],
],
'Table2' : [
    [20,4,4,aes],
    [23,4,5,aes],
    [26,4,6,aes],
    [29,6,4,aes],
    [20,4,4,lowmc],
    [23,4,5,lowmc],
    [26,4,6,lowmc],
    [29,6,4,lowmc],
],
'Figure8' : [
    [20,4,4,lowmc],
    [25,5,4,lowmc],
    [30,4,7,lowmc],
]
}
   

parser = argparse.ArgumentParser()
parser.add_argument("experiment", help="One of " + ', '.join(experiments.keys()))
parser.add_argument("pem_file", help="PEM file to ssh into DORAM_benchmark_ machines")
parser.add_argument("--queries", help="Num queries to run (default 100000)")
parser.add_argument("--threads", help="Num threads to use per party (default 20; this should be less than num cpu cores per machine)")
args = parser.parse_args()

if args.experiment not in experiments:
    print("Unrecognized experiment:", args.experiment)
    exit(1)

if args.queries is None:
    num_queries = 100000
else:
    num_queries = args.queries

if args.threads is None:
    num_threads = 20
else:
    num_threads = args.threads

for experiment in experiments[args.experiment]:
    #print(experiment, type(experiment))
    # unpack experiment
    log_N, num_levels, log_amp_factor, prf_circuit_file = experiment

    command = f"./run_3_server_experiment.sh {args.pem_file} --num-query-tests {num_queries} --log-address-space {log_N} --num-levels {num_levels} --num-threads {num_threads} --log-amp-factor {log_amp_factor} --prf-circuit-filename  {prf_circuit_file} --build-bottom-level-at-startup {'true' if log_N<20 else 'false'}"
    print(command, '\n')

    subprocess.run(command, capture_output=False, shell=True)