# Embedded Linux Systems — Assignment 01

**Topic:** How to Write a Device Driver  
**Student:** Huynh Nhan Thien Tri Tin  
**Student ID:** SE201630  
**Target:** QEMU `vexpress-a9` (ARMv7), Linux kernel 5.15.0

## Overview

This assignment implements and tests two Linux kernel character-device modules:

- **Task 1 — `asgn1_driver`:** a character device with basic read/write operations and four `ioctl` commands.
- **Task 2 — `counter_driver`:** a miscellaneous device with an auto-incrementing atomic counter and `/proc` statistics.

Both modules have been cross-compiled for ARMv7 and tested in a QEMU `vexpress-a9` environment. The functional tests described below have passed in the recorded test runs. The technical report and theory answers are separate assignment requirements and are not claimed as complete here.

## Project structure

```text
SE201630_TinHNTT_ASGN01/
├── task1_ioctl/
│   ├── asgn1_driver.c       # Character driver implementation
│   ├── asgn1_ioctl.h        # Shared ioctl definitions and statistics structure
│   ├── test_asgn1.c         # Userspace ioctl test program
│   ├── test_asgn1.sh        # QEMU test script
│   ├── Makefile             # ARMv7 kernel-module build
│   └── asgn1_driver.ko      # Built kernel module
├── task2_misc/
│   ├── counter_driver.c     # Misc device and /proc implementation
│   ├── test_counter.sh      # QEMU test script
│   ├── Makefile             # ARMv7 kernel-module build
│   └── counter_driver.ko    # Built kernel module
├── logs/                    # Test evidence, when collected
└── README.md
```

Generated build artifacts may be omitted from Git depending on `.gitignore`. The files shown above describe the intended working layout; the report, when completed, is submitted separately under the naming rules in the assignment brief.

## Task 1 — ioctl character driver

**Device:** `/dev/asgn1` (character device, major number `248`, minor `0` in the tested configuration).

| Command | Purpose |
| --- | --- |
| `ASGN1_RESET_BUFFER` | Clear the buffer and reset read/write counters. |
| `ASGN1_GET_STATS` | Return open count, read count, write count, buffer length and last-write size. |
| `ASGN1_SET_MODE` | Select Mode 0 (prefix written data with `[KERNEL]`) or Mode 1 (echo data unchanged). |
| `ASGN1_GET_VERSION` | Return the driver's version string (up to a 64-byte userspace buffer). |

The driver uses `unlocked_ioctl`, user-access helpers (`copy_to_user`/`copy_from_user`) and a mutex to protect shared driver state. Invalid ioctl commands return `-ENOTTY`.

## Task 2 — miscellaneous counter device

**Device:** `/dev/counter` (misc major `10`; minor is assigned dynamically).  
**Statistics:** `/proc/counter_info`.

- **Read:** returns the current counter as a decimal string with a trailing newline, then increments it by one.
- **Write:** accepts a non-negative decimal integer and sets the counter to that value. Negative or non-numeric input is rejected with `-EINVAL`.
- **Procfs:** reports the current counter value, total reads and total writes since module load.

The implementation uses `atomic_t` and related atomic operations. Read/write synchronization uses a mutex in the tested revision.

## Build environment

The tested environment uses:

- Ubuntu x86-64 as the build host.
- `arm-linux-gnueabihf-gcc` for ARMv7 cross-compilation.
- Linux 5.15 kernel source/build tree, configured for `ARCH=arm` and `CONFIG_ARCH_VEXPRESS=y`.
- `qemu-system-arm` with machine type `vexpress-a9`.
- A BusyBox initramfs with module-loading commands available.

The Makefiles expect the ARM kernel build tree at `~/embedded_lab1/kernel/linux-5.15`. Adjust `KDIR` in each Makefile if your local path differs.

### Build both modules (Ubuntu host)

From this assignment directory:

```bash
make -C task1_ioctl clean
make -C task1_ioctl
make -C task2_misc clean
make -C task2_misc

file task1_ioctl/asgn1_driver.ko task2_misc/counter_driver.ko
```

Both `.ko` files should be identified as **ELF 32-bit ARM** modules, built against the matching kernel configuration.

### Build the Task 1 userspace test (Ubuntu host)

```bash
arm-linux-gnueabihf-gcc -static -Wall \
  -o task1_ioctl/test_asgn1_arm \
  task1_ioctl/test_asgn1.c
```

## Run in QEMU

In the tested setup, the kernel image, device tree and original root filesystem are stored under `~/embedded_lab1/output/`. A prepared initramfs named `asgn1_initramfs.cpio.gz` includes the modules and test files under `/modules/`.

**Important:** After rebuilding or changing a file on the Ubuntu host, update the initramfs and restart QEMU. An already-running QEMU instance does not automatically receive changes made to the host-side image.

Start QEMU **on the Ubuntu host**:

```bash
cd ~/embedded_lab1
qemu-system-arm \
  -M vexpress-a9 \
  -m 256M \
  -nographic \
  -kernel output/zImage \
  -dtb output/vexpress-v2p-ca9.dtb \
  -initrd output/asgn1_initramfs.cpio.gz \
  -append "console=ttyAMA0 rdinit=/init"
```

At the **QEMU shell** (`/ #`), verify the test files and run the scripts **one at a time**:

```sh
ls -l /modules
sh /modules/test_asgn1.sh
echo "Task 1 exit code: $?"

sh /modules/test_counter.sh
echo "Task 2 exit code: $?"
```

Each script loads its module, creates the relevant device node, exercises the driver, and unloads the module. The expected result for each is a `TASK ... TEST PASSED` message and exit code `0`.

To exit QEMU in `-nographic` mode, press **Ctrl+A**, release, then press **X**.

## Verified test observations

The recorded QEMU ARMv7 sessions showed:

- **Task 1:** Kernel and Echo modes, RESET, GET_STATS, GET_VERSION, invalid ioctl handling (`ENOTTY`), and module cleanup passed.
- **Task 2:** auto-increment on read, setting the counter to `100`, resetting to `0`, rejection of negative/non-numeric writes, `/proc/counter_info` counters, and module cleanup passed.
