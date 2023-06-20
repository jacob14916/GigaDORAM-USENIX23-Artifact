#!/usr/bin/bash
ssh_key="$1"
test_name="doram"
party=$2
ip=$3
prev_host_and_port=$4
next_host_and_port=$5

echo $test_name $party $ip $prev_host_and_port $next_host_and_port

exe_name="test_$test_name"
exe_filepath="build/bin/$exe_name"

ssh -i $ssh_key "ubuntu@$ip" "killall $exe_name"
rsync -a -e "ssh -i $ssh_key" circuits "ubuntu@$ip:~"
scp -i $ssh_key $exe_filepath "ubuntu@$ip:~"

redirect_output="1> p$party.cout 2> p$party.cerr"
if [ "$party" = 1 ]
then
    redirect_output=""
    echo ssh -i $ssh_key "ubuntu@$ip" "./$exe_name $party $prev_host_and_port $next_host_and_port ./circuits ${@:6} $redirect_output"
fi

ssh -i $ssh_key "ubuntu@$ip" "./$exe_name $party $prev_host_and_port $next_host_and_port ./circuits ${@:6} $redirect_output"

if [ "$party" -ne 1 ]
then
    scp -i $ssh_key "ubuntu@$ip:~/p$party.cout" .
    scp -i $ssh_key "ubuntu@$ip:~/p$party.cerr" .
fi
local_timing_file="./multi_server_results/doram_timing_report$party-last.txt"
scp -i $ssh_key "ubuntu@$ip:~/doram_timing_report$party.txt" $local_timing_file
main_timing_file="./multi_server_results/doram_timing_report$party.txt"
touch "$main_timing_file"
echo "---------" >> "$main_timing_file"
echo "Data from" >> "$main_timing_file"
TZ="America/Los_Angeles" date >> "$main_timing_file"
cat "$local_timing_file" >> "$main_timing_file"