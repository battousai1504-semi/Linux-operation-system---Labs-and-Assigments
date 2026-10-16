#!/bin/sh
# test_sms.sh - ASSIGNMENT-02: kiểm thử driver + sms_app trên QEMU
# In ra các khối có đánh dấu === XXX START/END === để tách log sau này.
MOD=/lib/modules/$(uname -r)/sms_sensor_driver.ko

echo '=== DRIVER_TEST START ==='
grep -q '^sms_sensor_driver ' /proc/modules && rmmod sms_sensor_driver
insmod $MOD || { echo 'ERROR: insmod failed'; exit 1; }
lsmod | grep sms
ls -l /dev/sms_sensor /proc/sms_stats
echo '--- head -n 3 /dev/sms_sensor (blocking read, interval 100 ms):'
head -n 3 /dev/sms_sensor
cat /proc/sms_stats
echo '=== DRIVER_TEST END ==='

echo '=== EXITCODE_TEST START ==='
sms_app -i abc
echo "exit code (invalid argument) = $?"
sms_app -D /dev/not_exist -d 1
echo "exit code (device not found) = $?"
echo '=== EXITCODE_TEST END ==='

# Nạp lại driver để read_count bắt đầu từ 0 cho lần chạy 60 giây
rmmod sms_sensor_driver
insmod $MOD
rm -f /tmp/sms.log /tmp/sms.log.1

echo '=== PROC_BEFORE START ==='
cat /proc/sms_stats
echo '=== PROC_BEFORE END ==='

sms_app -d 60 -v > /tmp/sms_run.txt 2>&1 &
PID=$!
echo "sms_app started in background, pid=$PID (60 s)"

sleep 5
echo '=== MEM_T5 START ==='
grep -E 'VmPeak|VmSize|VmLck|VmRSS|VmData|VmStk|Threads' /proc/$PID/status
echo '=== MEM_T5 END ==='

sleep 25
echo '=== MEM_T30 START ==='
grep -E 'VmPeak|VmSize|VmLck|VmRSS|VmData|VmStk|Threads' /proc/$PID/status
echo '=== MEM_T30 END ==='
echo '=== THREADS START ==='
for T in /proc/$PID/task/*; do
    awk '{ printf "tid=%-5s %-15s rt_priority=%-3s policy=%s\n", $1, $2, $40, ($41==1 ? "SCHED_FIFO" : "SCHED_OTHER") }' $T/stat
done
echo '=== THREADS END ==='
echo '=== MAPS_T30 START ==='
cat /proc/$PID/maps
echo '=== MAPS_T30 END ==='
echo '=== PROC_DURING START ==='
cat /proc/sms_stats
echo '=== PROC_DURING END ==='

sleep 25
echo '=== MEM_T55 START ==='
grep -E 'VmPeak|VmSize|VmLck|VmRSS|VmData|VmStk|Threads' /proc/$PID/status
echo '=== MEM_T55 END ==='

wait $PID
echo "sms_app exit code = $?"

echo '=== PROC_AFTER START ==='
cat /proc/sms_stats
echo '=== PROC_AFTER END ==='

echo '=== SAMPLE_RUN START ==='
cat /tmp/sms_run.txt
echo '=== SAMPLE_RUN END ==='

echo '=== SMS_LOG START ==='
cat /tmp/sms.log
echo '=== SMS_LOG END ==='

echo '=== SIGINT_TEST START ==='
sms_app -d 0 -L /tmp/sms_sigint.log > /tmp/sms_sigint.txt 2>&1 &
PID=$!
sleep 3
kill -INT $PID
wait $PID
echo "exit code after SIGINT = $?"
cat /tmp/sms_sigint.txt
echo "--- /dev/mqueue after exit (empty = queues removed):"
ls /dev/mqueue
echo '=== SIGINT_TEST END ==='

echo '=== DMESG START ==='
dmesg | grep sms_sensor
echo '=== DMESG END ==='
echo 'test_sms.sh finished'
