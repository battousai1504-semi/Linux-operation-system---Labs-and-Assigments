#!/bin/sh
# run_qemu.sh - tao initramfs_asgn2.cpio.gz va boot QEMU vexpress-a9 (ASSIGNMENT-02)
# Author : Nguyen Tran Quang - SE200988
#
# Can co (sua bien ben duoi neu duong dan khac):
#   KDIR   : kernel 5.15 LAB-01 da build lai voi CONFIG_POSIX_MQUEUE=y
#   ROOTFS : rootfs co bin/busybox (mac dinh: rootfs Lab01 trong chinh repo nay)
#   DTB    : vexpress-v2p-ca9.dtb (mac dinh: Lab01/.../output trong repo)
# Cach dung:  ./run_qemu.sh            -> vao shell, tu chay lenh
#             ./run_qemu.sh autotest   -> tu chay test_sms.sh + jitter_test.sh roi tat
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ASGN=$(dirname "$HERE")
LAB1=$ASGN/../../Lab01/SE201613_PhamLeHaiDang_Lab01
KDIR=${KDIR:-$HOME/embedded_lab1/kernel/linux-5.15}
ROOTFS=${ROOTFS:-$LAB1/rootfs/initramfs}
DTB=${DTB:-$LAB1/output/vexpress-v2p-ca9.dtb}
WORK=/tmp/asgn2_rootfs

# 1. Gop rootfs LAB-01 + file cua ASSIGNMENT-02
rm -rf $WORK
cp -a "$ROOTFS" $WORK
cp -a "$HERE/rootfs_overlay/." $WORK/
mkdir -p $WORK/lib/modules/5.15.0 $WORK/usr/bin $WORK/sbin $WORK/proc $WORK/sys $WORK/dev $WORK/tmp
cp "$ASGN/driver/sms_sensor_driver.ko" $WORK/lib/modules/5.15.0/
cp "$ASGN/app/sms_app" $WORK/usr/bin/
ln -sf /bin/busybox $WORK/sbin/init
ln -sf busybox $WORK/bin/sh
chmod +x $WORK/etc/init.d/rcS $WORK/usr/bin/*.sh

# 2. Dong goi initramfs
(cd $WORK && find . | cpio -H newc -o --owner root:root 2>/dev/null | gzip) > "$ASGN/output/initramfs_asgn2.cpio.gz"
ls -lh "$ASGN/output/initramfs_asgn2.cpio.gz"

# 3. Boot
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
    -kernel "$KDIR/arch/arm/boot/zImage" -dtb "$DTB" \
    -initrd "$ASGN/output/initramfs_asgn2.cpio.gz" \
    -append "console=ttyAMA0,115200 rdinit=/sbin/init $1"
