#!/bin/sh
set -eu

MODULE="/modules/counter_driver.ko"
DEVICE="/dev/counter"
PROC="/proc/counter_info"
LOADED=0

cleanup() {
    if [ "$LOADED" -eq 1 ]; then
        rmmod counter_driver || true
    fi
}

trap cleanup EXIT

check_stat() {
    if ! grep -Fxq "$1" "$PROC"; then
        echo "FAIL: Expected $1"
        cat "$PROC"
        exit 1
    fi
}

echo "=== TASK 2: COUNTER DRIVER TEST ==="

# Step 1: Load module
echo "=== LOAD MODULE ==="

if grep -q '^counter_driver ' /proc/modules; then
    echo "FAIL: Module already loaded"
    exit 1
fi

insmod "$MODULE"
LOADED=1

# Step 2: Create device node
echo "=== CREATE DEVICE NODE ==="

MINOR=$(awk '$2 == "counter" {print $1}' /proc/misc)

if [ -z "$MINOR" ]; then
    echo "FAIL: Counter misc device not registered"
    exit 1
fi

[ -e "$DEVICE" ] || mknod "$DEVICE" c 10 "$MINOR"
ls -l "$DEVICE"

# Step 3: Initial statistics
echo "=== INITIAL STATS ==="

cat "$PROC"
check_stat "Current counter: 0"
check_stat "Total reads: 0"
check_stat "Total writes: 0"

# Step 4: Read and auto-increment
echo "=== READ TEST ==="

VALUE=$(cat "$DEVICE")
[ "$VALUE" = "0" ] || {
    echo "FAIL: First read expected 0, got $VALUE"
    exit 1
}

VALUE=$(cat "$DEVICE")
[ "$VALUE" = "1" ] || {
    echo "FAIL: Second read expected 1, got $VALUE"
    exit 1
}

echo "PASS: Read and auto-increment"

check_stat "Current counter: 2"
check_stat "Total reads: 2"

# Step 5: Write 100
echo "=== WRITE TEST ==="

echo 100 > "$DEVICE"

cat "$PROC"
check_stat "Current counter: 100"
check_stat "Total writes: 1"

VALUE=$(cat "$DEVICE")
[ "$VALUE" = "100" ] || {
    echo "FAIL: Expected 100, got $VALUE"
    exit 1
}

echo "PASS: Write 100"

# Step 6: Reject invalid values
echo "=== INVALID INPUT TEST ==="

if (echo -5 > "$DEVICE") 2>/dev/null; then
    echo "FAIL: Negative value accepted"
    exit 1
fi

if (echo abc > "$DEVICE") 2>/dev/null; then
    echo "FAIL: Invalid string accepted"
    exit 1
fi

check_stat "Current counter: 101"
check_stat "Total writes: 1"

echo "PASS: Negative and invalid inputs rejected"

# Step 7: Reset to zero
echo "=== RESET TEST ==="

echo 0 > "$DEVICE"

cat "$PROC"
check_stat "Current counter: 0"
check_stat "Total reads: 3"
check_stat "Total writes: 2"

VALUE=$(cat "$DEVICE")
[ "$VALUE" = "0" ] || {
    echo "FAIL: Reset did not return 0"
    exit 1
}

echo "PASS: Reset to zero"

# Step 8: Final statistics
echo "=== FINAL STATS ==="

cat "$PROC"
check_stat "Current counter: 1"
check_stat "Total reads: 4"
check_stat "Total writes: 2"

# Step 9: Unload module
echo "=== UNLOAD MODULE ==="

rmmod counter_driver
LOADED=0

# Step 10: Verify cleanup
echo "=== KERNEL LOG ==="

dmesg | tail -n 15

dmesg | grep -Fq "counter: module unloaded"

echo "=== TASK 2 TEST PASSED ==="