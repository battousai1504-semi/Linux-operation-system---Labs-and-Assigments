#!/bin/sh
# jitter_test.sh - ASSIGNMENT-02 Task 4: so sánh jitter SCHED_FIFO vs SCHED_OTHER
# Mỗi lần chạy: chu kỳ 10 ms trong 15 s (~1500 mẫu). Chạy 4 lần:
#   không tải: fifo, other   |   có tải (4 vòng lặp chiếm CPU): fifo, other
INTERVAL=${1:-10}
DUR=${2:-15}
MOD=/lib/modules/$(uname -r)/sms_sensor_driver.ko

grep -q '^sms_sensor_driver ' /proc/modules || insmod $MOD

echo '=== JITTER_TEST START ==='
echo "interval=${INTERVAL} ms, duration=${DUR} s per run, CPUs=$(grep -c ^processor /proc/cpuinfo)"
for LOAD in 0 4; do
    PIDS=''
    i=0
    while [ $i -lt $LOAD ]; do
        sh -c 'while :; do :; done' &
        PIDS="$PIDS $!"
        i=$((i + 1))
    done
    for POL in fifo other; do
        sms_app -i $INTERVAL -d $DUR -s $POL -t 100 -h 100 -L /tmp/sms_jitter.log \
            > /tmp/jitter_${POL}_${LOAD}.txt 2>&1
        RC=$?
        echo ""
        echo "########## policy=$POL  background_load=$LOAD busy loop(s)  (exit code $RC) ##########"
        sed -n '/===== JITTER REPORT/,/>1000us/p' /tmp/jitter_${POL}_${LOAD}.txt
    done
    [ -n "$PIDS" ] && kill $PIDS
done
echo '=== JITTER_TEST END ==='
