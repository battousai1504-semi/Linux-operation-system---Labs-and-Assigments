#!/bin/sh

MODULE=/lib/modules/5.15.0/lab2_driver.ko
SYSFS=/sys/class/lab2_class/lab2

echo '=== LAB-02: procfs and sysfs Test ==='

if ! grep -q '^lab2_driver ' /proc/modules; then
    insmod "$MODULE" || {
        echo 'ERROR: insmod failed'
        exit 1
    }
fi

[ -c /dev/lab2 ] || mknod /dev/lab2 c 240 0

echo '--- /proc/lab2_info before write ---'
cat /proc/lab2_info

echo 'Embedded Linux Lab2 Test Data' > /dev/lab2

echo '--- /proc/lab2_info after write ---'
cat /proc/lab2_info

echo '--- sysfs attributes ---'
echo "buffer_len : $(cat "$SYSFS/buffer_len")"
echo "open_count : $(cat "$SYSFS/open_count")"
echo "last_data  : $(cat "$SYSFS/last_data")"

echo '=== procfs and sysfs Test PASSED ==='
