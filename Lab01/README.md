# Lab 01 — Embedded Linux ARMv7

Bài nộp theo yêu cầu mục V của đề LAB-01.

- `SE201613_PhamLeHaiDang_Lab01.tar.gz`: `output/`, `configs/`, `rootfs/initramfs/`, và báo cáo PDF. `output/u-boot` được kèm thêm để kiểm tra phần B.
- `SE201613_Lab01_BaoCao.pdf`: bản báo cáo PDF để xem trực tiếp trên GitHub.

Giải nén bằng `tar -xzf SE201613_PhamLeHaiDang_Lab01.tar.gz`. Có thể khởi động bộ artifact trong thư mục đã giải nén bằng:

```sh
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel output/zImage -dtb output/vexpress-v2p-ca9.dtb \
  -initrd output/initramfs.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'
```

Trong kho lưu trữ nén có các device node dưới `rootfs/initramfs/dev/`; cần giải nén với quyền phù hợp nếu muốn tái tạo các node đó trên filesystem.
