#!/usr/bin/bash
# Benchmark a test on a single machine with the given network settings simulated by tc
latency=$1
bandwidth=$2
additional_args=${@:3}

cleanup ()
{
    echo "Interrupted; unsetting qdisc"
    sudo tc qdisc del dev lo root
    exit 130
}

trap cleanup SIGINT

sudo tc qdisc replace dev lo root netem delay "$latency" rate "$bandwidth"

./run_rep.sh doram ./circuits $additional_args

for i in 1 2 3; do
    main_timing_file="./single_server_results/doram_timing_report${i}.txt"
    touch "$main_timing_file"

    echo "---------" >>"$main_timing_file"
    echo "Data from" >>"$main_timing_file"
    TZ="America/Los_Angeles" date >>"$main_timing_file"
    echo "---------" >>"$main_timing_file"
    echo "Latency: $latency" >>"$main_timing_file"
    echo "Bandwidth: $bandwidth" >>"$main_timing_file"
    cat "doram_timing_report${i}.txt" >>"$main_timing_file"
done

sudo tc qdisc del dev lo root