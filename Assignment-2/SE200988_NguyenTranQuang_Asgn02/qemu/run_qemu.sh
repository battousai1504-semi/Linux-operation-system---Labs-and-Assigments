#!/bin/sh
# run_qemu.sh - đóng gói rootfs thành initramfs_asgn2.cpio.gz rồi boot QEMU (ASSIGNMENT-02)
# zImage: bản build lại có CONFIG_POSIX_MQUEUE + CONFIG_HIGH_RES_TIMERS (Bước 2)
set -e
LAB1=$HOME/embedded_lab1/output
A=$HOME/asgn2
cd "$A/rootfs/initramfs"
find . | cpio -H newc -o --owner root:root | gzip > "$A/output/initramfs_asgn2.cpio.gz"
ls -lh "$A/output/initramfs_asgn2.cpio.gz"
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
    -kernel "$A/output/zImage" \
    -dtb "$LAB1/vexpress-v2p-ca9.dtb" \
    -initrd "$A/output/initramfs_asgn2.cpio.gz" \
    -append "console=ttyAMA0,115200 rdinit=/sbin/init mem=512M"
