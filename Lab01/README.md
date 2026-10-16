# Lab 01 — Embedded Linux ARMv7

Thư mục [`SE201613_PhamLeHaiDang_Lab01/`](SE201613_PhamLeHaiDang_Lab01/) là cây bài nộp có thể xem trực tiếp trên GitHub:

- `output/`: zImage, DTB, initramfs và U-Boot.
- `configs/`: cấu hình kernel và BusyBox.
- `rootfs/initramfs/`: cây root filesystem và các symlink BusyBox.
- `bao_cao/`: báo cáo PDF và bản Word nguồn.
- `logs/`: log build và boot để đối chiếu.

[`SE201613_PhamLeHaiDang_Lab01.tar.gz`](SE201613_PhamLeHaiDang_Lab01.tar.gz) là gói nộp theo mục V của đề. Bản nén giữ được các Linux device node trong `rootfs/initramfs/dev/`, vốn Git không lưu được. [`SE201613_Lab01_BaoCao.pdf`](SE201613_Lab01_BaoCao.pdf) là bản báo cáo để xem nhanh.

Boot từ các artifact trong thư mục đã clone:

```sh
cd SE201613_PhamLeHaiDang_Lab01
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel output/zImage -dtb output/vexpress-v2p-ca9.dtb \
  -initrd output/initramfs.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'
```
