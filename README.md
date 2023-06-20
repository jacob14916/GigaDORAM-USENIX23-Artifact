# GigaDORAM USENIX Security 2023 Artifact

- [GigaDORAM USENIX Security 2023 Artifact](#gigadoram-usenix-security-2023-artifact)
    - [Authors (listed alphabetically)](#authors-listed-alphabetically)
  - [Single machine tests](#single-machine-tests)
    - [Requirements](#requirements)
    - [Basic setup](#basic-setup)
    - [Reproducing Figures 6a and 6b](#reproducing-figures-6a-and-6b)
    - [Run any combination of parameters](#run-any-combination-of-parameters)
  - [Multi machine tests](#multi-machine-tests)
    - [Requirements for multi-machine tests](#requirements-for-multi-machine-tests)
    - [Setup](#setup)
    - [Reproducing Figures 5 and 8 and Tables 1 and 2](#reproducing-figures-5-and-8-and-tables-1-and-2)
    - [Testing particular parameters on 3 servers](#testing-particular-parameters-on-3-servers)


### Authors (listed alphabetically)
* Brett Hemenway Falk, `fbrett at cis dot upenn dot edu`
* Rafail Ostrovsky, `rafail at cs dot ucla dot edu`
* Matan Shtepel, `matan dot shtepel at gmail dot com`
* Jacob Zhang, `jacob dot b dot zhang at gmail dot com`

This repo contains the artifact for the paper "GigaDORAM: Breaking the Billion Address Barrier" published in USENIX Security 2023. We provide directions and Python scripts for reproducing the experiments measuring GigaDORAM in the paper.

## Single machine tests

### Requirements 

* A Linux machine with processor supporting the Intel SSE2 instruction set. 
* `sudo` access is unfortunately required; this isn't an issue on AWS. 
* For compilation, the EMP-toolkit library. Follow the instructions at https://github.com/emp-toolkit/emp-tool to install EMP and its dependencies. 
  * EMP's dependencies are extremely basic (`python3 cmake git build-essential libssl-dev`) and are all needed to build GigaDORAM.
* On a typical Linux system there are no dependencies needed to run the compiled binary.
* We recommend at least 8 CPU cores and 8GB of RAM. 


### Basic setup

1. Clone this repository: 
```
git clone https://github.com/jacob14916/GigaDORAM-USENIX23-Artifact 
```
2. Run the compile script. If this fails, make sure you've installed EMP-toolkit.
```
./compile.sh
```

### Reproducing Figures 6a and 6b
The single server experiments are run by `./single_server_experiments.py` which prints usage information with the `-h` flag:
```
./single_server_experiments.py -h
```
To reproduce the tests used to generate the GigaDORAM data in Figures 6a and 6b in the paper, run
```
$ ./single_server_experiments.py Figure6a
$ ./single_server_experiments.py Figure6b
```
We ran these tests on a machine with 96 CPUs and saw best results with 20 threads per party (which is the default). If you are running on a smaller machine, you should use a num_threads which is less than 1/3 the number of CPUs available:
```
$ ./single_server_experiments.py Figure6a --threads 2
$ ./single_server_experiments.py Figure6b --threads 2
```

The concatenated output from all experiments will be written to `single_server_results/doram_timing_report${i}.txt` for party i in 1, 2, 3.

These files are are human readable and formatted in blocks as follows (here is an example):
```
DORAM Parameters
Number of queries: 1000
Build bottom level at startup: 0
Log address space size: 16
Data block size (bits): 64
Log linear level size: 8
Log amp factor: 4
Num levels: 3
PRF circuit file: LowMC_reuse_wires.txt
Num threads: 1

Timing Breakdown
Total time including builds: 2.89467e+06 us
Time spent in queries: 2.83478e+06 us
Time spent in query PRF eval: 1.21668e+06 us
Time spent querying linear level: 751191 us
Time spent in build PRF eval: 8775 us
Time spent in batcher sorting: 0 us
Time spent building bottom level: 0 us
Time spent building other levels: 20811 us 

SUMMARY
Total time including builds: 2.89467e+06 us 
Total number of bytes sent: 2.7828e+07
Queries/sec: 345.462
```
The SUMMARY section is the most important to look at, as it gives the total time and communication to run the test.


  * WARNING: single_server_experiments.py runs `sudo tc qdisc replace dev lo root` to simulate network latency and bandwidth limits on the loopback interface, which can slow down other programs running on the machine. We recommend running on a dedicated VM. If anything strange happens, the network changes can be undone with `sudo tc qdisc del dev lo root`

### Run any combination of parameters
To dig deeper and test a particular combination of network speed and DORAM parameters, use the following commands, shown here with an example set of arguments:
```
./benchmark_doram_locally.sh 100us 10Gbit --prf-circuit-filename LowMC_reuse_wires.txt --build-bottom-level-at-startup false --num-query-tests 100000 --log-address-space 16 --num-levels 3 --log-amp-factor 4 --num-threads 1
```
The script passes its first two arguments, representing network delay and bandwidth, to `tc` as in the following example:
```
sudo tc qdisc replace dev lo root netem delay 100us rate 10Gbit
```
This is unset on interrupt (Ctrl+C) or normal script exit. You can run `man tc` to view documentation.

The remaining arguments to `benchmark_doram_locally.sh` mean:
* `--prf-circuit-filename` specifies the circuit file (stored under `circuits`) for the block cipher to use as the oblivious PRF. Options we provide are `LowMC_reuse_wires.txt` for an (optimized) LowMC, and `aes_128_extended.txt` for AES. The AES circuit file is the widely used one found at https://homes.esat.kuleuven.be/~nsmart/MPC/MAND/aes_128.txt, while the LowMC file is our own work (with thanks to the LowMC authors.) 
* `--build-bottom-level-at-startup` must be one of `true` or `false`, and determines whether to build the bottom level in the DORAM (whose size is equal to the address space size of the DORAM) at the beginning of the test run (`true`), or defer building it until it becomes necessary (`false`). Deferring the bottom level build comes at a moderate performance penalty when the build finally happens, but is necessary to practically test the DORAM at large address space sizes.
* `--num-query-tests` specifies the number of queries (reads or writes) to run in the test for timing purposes.
* `--log-address-space` is the log base 2 of the address space size of the DORAM. For example `--log-address-space 16` specifies a DORAM of size $2^{16} - 1$, indexed by indices from $1$ to $2^{16}-1$.
* `--num-levels` is the number of levels in the DORAM hierarchy.
* `--log-amp-factor` is the log base 2 of the amplification factor, i.e. the ratio of sizes of successive levels in the hierarchy.
* For example, `--log-address-space 16 --num-levels 3 --log-amp-factor 4` specifies a DORAM of size $2^{16}$ with 3 levels of size $2^8$, $2^{12}$, and $2^{16}$.
* `--num-threads` is the number of threads per DORAM party to use for multithreaded computations. This number should be less than 1/3 the number of available CPUs if running all 3 parties on the same machine.

There is also a script `run_doram.sh` taking the arguments described above that does not perform any network modifications.

If the computation succeeds, each party's most important output will be written to the file named `doram_timing_report${party}.txt` (e.g. `doram_timing_report1.txt`).

The format of these output files is described in the previous section. Additionally, some basic messages are written to stdout, and more verbose output is written to stderr; party 1 prints these on the command line, while party 2 writes to `p2_output.txt` and `p2.cerr`, and similarly for party 3.

## Multi machine tests

### Requirements for multi-machine tests
*Warning*: Follow the directions below carefully; it is easy to miss something which will cause the benchmarks to not be able to run.

* The multi-machine tests are designed to be run on AWS EC2.
* You will need choose an AWS region and create and start AWS EC2 instances named `DORAM_benchmark_1`, `DORAM_benchmark_2`, `DORAM_benchmark_3` in that region.
  * Use the same SSH key for access to all 3 instances, as it will be an argument to the script later. 
  * Set security group settings to allow TCP traffic between the 3 instances.
  * We used `c5n.metal` instances, which guaranteed that our parties were not running on the same physical host, and also provided high multi-threaded performance.
  * We used Ubuntu 22.04 but any modern enough Linux distribution should work.
* For most tests, the instances should be created in a cluster placement group. A quick how to is covered in the video tutorial. For more information about cluster placement groups, see [here](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/placement-groups.html).

### Setup
1. In addition to the requirements for single server tests, the AWS CLI (package `awscli` in apt) needs to be installed on the machine used for builds.
2. After installing, configure your region and access keys with `aws configure`.
3. Additionally, you will need to add these lines to `~/.ssh/config/` on the build machine:
```
Host *
        StrictHostKeyChecking no
```
Without disabling StrictHostKeyChecking, the experiment script will be unable to ssh to new hosts in the background, since user input would be required to continue connecting to a new host.

4. Nothing needs to be installed on the benchmark instances!

### Reproducing Figures 5 and 8 and Tables 1 and 2
```
$ ./multi_server_experiments.py -h
usage: multi_server_experiments.py [-h] [--queries QUERIES] [--threads THREADS] experiment pem_file

positional arguments:
  experiment         One of Figure5, Table1, Table2, Figure8
  pem_file           PEM file to ssh into DORAM_benchmark_ machines

options:
  -h, --help         show this help message and exit
  --queries QUERIES  Num queries to run (default 100000)
  --threads THREADS  Num threads to use per party (default 20; this should be less than num cpu cores per machine)

$ ./multi_server_experiments.py Figure5 ../my_pem_file.pem
$ ./multi_server_experiments.py Table1 ../my_pem_file.pem
$ ./multi_server_experiments.py Table2 ../my_pem_file.pem
$ ./multi_server_experiments.py Figure8 ../my_pem_file.pem
```

The concatenated output from all experiments will be written to `multi_server_results/doram_timing_report${i}.txt` for party i in 1, 2, 3, following the same format as the single server results.

### Testing particular parameters on 3 servers
The named arguments to `run_3_server_experiment.sh` are the same as for `benchmark_doram_locally.sh`. Instead of passing network delay and bandwidh, there is one unnamed argument, which must come first, that specifies the PEM file to ssh into the DORAM_benchmark_ machines. Example:
```
./run_3_server_experiment.sh ../my_key.pem --prf-circuit-filename LowMC_reuse_wires.txt --build-bottom-level-at-startup false --num-query-tests 100000 --log-address-space 16 --num-levels 3 --log-amp-factor 4 --num-threads 1
```