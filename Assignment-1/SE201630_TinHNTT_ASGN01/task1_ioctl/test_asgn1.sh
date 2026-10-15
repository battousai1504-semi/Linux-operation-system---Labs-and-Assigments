#!/bin/sh
set -eu

MODULE="/modules/asgn1_driver.ko"
TEST="/modules/test_asgn1_arm"
DEVICE="/dev/asgn1"
LOADED=0

cleanup() {
    if [ "$LOADED" -eq 1 ]; then
        rmmod asgn1_driver || true
    fi
}

trap cleanup EXIT

echo "=== TASK 1: IOCTL DRIVER TEST ==="

# Step 1: Load module
echo "=== LOAD MODULE ==="

if grep -q '^asgn1_driver ' /proc/modules; then
    echo "FAIL: Module already loaded"
    exit 1
fi

insmod "$MODULE"
LOADED=1

# Step 2: Create device node
echo "=== CREATE DEVICE NODE ==="

MAJOR=$(awk '$2 == "asgn1" {print $1}' /proc/devices)

if [ "$MAJOR" != "248" ]; then
    echo "FAIL: Incorrect major number"
    exit 1
fi

[ -e "$DEVICE" ] || mknod "$DEVICE" c "$MAJOR" 0
ls -l "$DEVICE"

# Step 3: Run userspace ioctl tests
echo "=== RUN IOCTL TESTS ==="

OUTPUT=$("$TEST" 2>&1) || {
    printf '%s\n' "$OUTPUT"
    echo "FAIL: Userspace test failed"
    exit 1
}

printf '%s\n' "$OUTPUT"

# Step 4: Verify test results
printf '%s\n' "$OUTPUT" | grep -Fq \
    "Read data: [KERNEL]Hello Linux"

printf '%s\n' "$OUTPUT" | grep -Fq \
    "PASS: RESET cleared buffer and counters"

printf '%s\n' "$OUTPUT" | grep -Fq \
    "PASS: Echo mode - Hello Linux"

printf '%s\n' "$OUTPUT" | grep -Fq \
    "PASS: Invalid ioctl returns ENOTTY"

printf '%s\n' "$OUTPUT" | grep -Fq \
    "Version: ASGN1 Driver Version 1.0"

# Step 5: Unload module
echo "=== UNLOAD MODULE ==="

rmmod asgn1_driver
LOADED=0

# Step 6: Verify cleanup
echo "=== KERNEL LOG ==="
dmesg | tail -n 20

dmesg | grep -Fq "asgn1: device unregistered"

echo "=== TASK 1 TEST PASSED ==="