#!/bin/sh
# jitter_test.sh - ASSIGNMENT-02 Task 4: so sanh jitter SCHED_FIFO va SCHED_OTHER
INTERVAL=${1:-10}
DUR=${2:-15}
MOD=/lib/modules/$(uname -r)/sms_sensor_driver.ko

grep -q '^sms_sensor_driver ' /proc/modules || insmod $MOD

echo '=== JITTER_TEST START ==='
echo "interval=${INTERVAL} ms, duration=${DUR} s, CPUs=$(grep -c ^processor /proc/cpuinfo)"
for LOAD in 0 2; do
    PIDS=''
    i=0
    while [ $i -lt $LOAD ]; do
        sh -c 'while :; do :; done' &
        PIDS="$PIDS $!"
        i=$((i + 1))
    done
    for POL in fifo other; do
        sms_app -i $INTERVAL -d $DUR -s $POL -t 100 -h 100 -L /tmp/sms_jitter.log > /tmp/jit.txt 2>&1
        echo ""
        echo "##### policy=$POL  background_load=$LOAD busy loop(s) #####"
        sed -n '/===== JITTER REPORT/,/>1000us/p' /tmp/jit.txt
        grep 'samples  ' /tmp/jit.txt
    done
    [ -n "$PIDS" ] && kill $PIDS
done
echo '=== JITTER_TEST END ==='
