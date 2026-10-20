#!/bin/sh

MODULE=/lib/modules/5.15.0/lab2_driver.ko

echo '=== LAB-02: Character Device Test ==='

if ! grep -q '^lab2_driver ' /proc/modules; then
    echo '[1] Loading lab2_driver...'
    insmod "$MODULE" || {
        echo 'ERROR: insmod failed'
        exit 1
    }
else
    echo '[1] lab2_driver is already loaded'
fi

[ -c /dev/lab2 ] || mknod /dev/lab2 c 240 0

echo '[2] Module and device node:'
lsmod | grep lab2
ls -l /dev/lab2

echo '[3] Write/read test:'
echo 'Hello from userspace, LAB-02!' > /dev/lab2
DATA="$(cat /dev/lab2)"
echo "Read back: [$DATA]"

echo '[4] Multiple write/read test:'
for i in 1 2 3
do
    echo "Message_$i" > /dev/lab2
    echo "Read $i: [$(cat /dev/lab2)]"
done

echo '[5] Recent kernel messages:'
dmesg | grep lab2 | tail -20

echo '=== Character Device Test PASSED ==='
