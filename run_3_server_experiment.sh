#!/usr/bin/bash
NAME_PREFIX=DORAM_benchmark_
party_script=$(dirname $0)/party.sh
ssh_key=$1
test_name="doram"

if [ -z "$test_name" ] ; then
echo "Which test did you want to run?"
exit 1
fi

if [ $(basename $(pwd)) != 'DORAM' ] ; then
echo "Run this from the DORAM directory"
exit 1
fi

for i in 1 2 3 ; do
public_ip_i=$(aws ec2 describe-instances --filter "Name=tag:Name,Values=$NAME_PREFIX$i" --query 'Reservations[0].Instances[0].PublicIpAddress' | tr -d '"')
private_ip_i=$(aws ec2 describe-instances --filter "Name=tag:Name,Values=$NAME_PREFIX$i" --query 'Reservations[0].Instances[0].PrivateIpAddress' | tr -d '"')
if [ $public_ip_i = 'null' ] ; then
echo "Instance $i not running"
exit 1
fi
eval "public_ip_$i=$public_ip_i"
eval "private_ip_$i=$private_ip_i"
done

$party_script $ssh_key 2 $public_ip_2 "$private_ip_1:1200" "$private_ip_3:2300" ${@:2} &
p2_pid="$!"
$party_script $ssh_key 3 $public_ip_3 "$private_ip_2:2300" "$private_ip_1:1300" ${@:2} &
p3_pid="$!"
$party_script $ssh_key 1 $public_ip_1 "$private_ip_3:1300" "$private_ip_2:1200" ${@:2} 
p1_pid="$!"

wait $p1_pid $p2_pid $p3_pid