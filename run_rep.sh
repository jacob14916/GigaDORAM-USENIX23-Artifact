#!/bin/bash
killall "test_$1"
prog="build/bin/test_$1"
sleep 0.05
$prog 2 1200 2300 ${@:2} 1> p2_output.txt 2> p2.cerr & 
p2_pid="$!"
$prog 3 2300 1300 ${@:2} 1> p3_output.txt 2> p2.cerr &
p3_pid="$!"
# p1 runs in the main terminal
if [[ $0 = *"gdb"* ]] ; then
    gdb -q -ex=r --args $prog 1 1300 1200 ${@:2}
    #-ex='break sh_rep_bin.h:155'
    #-ex='break OHTable'
else
    $prog 1 1300 1200 ${@:2} 
fi
# ! if we don't wait for all 3 processes, the check script may run on incomplete output
wait $p3_pid $p2_pid
