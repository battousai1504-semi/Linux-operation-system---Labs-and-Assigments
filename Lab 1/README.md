# Lab 1 — Cấu hình Kernel và khởi động Embedded Linux ARMv7

Bài thực hành LOS201 chạy Linux 5.15 trên máy ảo QEMU `vexpress-a9` (ARMv7-A, Cortex-A9). Repo lưu trực tiếp các artifact, cấu hình, cây root filesystem, log và báo cáo để có thể xem từng phần mà không phải giải nén một gói bài nộp.

**Kết quả:** kernel `zImage` và Device Tree được build thành công; U-Boot 2022.04 khởi động vào shell; BusyBox 1.35.0 được build static; hệ thống boot từ initramfs tới BusyBox shell với 2 CPU mô phỏng. Xem [báo cáo PDF](bao_cao/SE201613_Lab01_BaoCao.pdf) để có ảnh chụp, giải thích và câu trả lời phân tích.

## 1. Mục tiêu và phạm vi

| Phần trong đề | Công việc | Kết quả trong repo |
| --- | --- | --- |
| A — Linux kernel | Cross-compile kernel ARMv7, cấu hình VExpress, build `zImage` và DTB | `output/zImage`, `output/vexpress-v2p-ca9.dtb`, `configs/kernel.config` |
| B — U-Boot | Build U-Boot cho VExpress CA9, vào shell, khảo sát `printenv`, đặt `bootargs`/`bootcmd`, quan sát kernel panic khi chưa có rootfs | `output/u-boot`, `logs/partB_uboot.log`, báo cáo PDF |
| C — Rootfs và initramfs | Build BusyBox static, tạo rootfs tối giản, đóng gói cpio, boot QEMU tới shell | `rootfs/initramfs/`, `output/initramfs.cpio.gz`, `configs/busybox.config`, `logs/partC_smp*.log` |
| Báo cáo | Sơ đồ boot, minh chứng A/B/C, 5 câu hỏi phân tích và khó khăn gặp phải | `bao_cao/SE201613_Lab01_BaoCao.pdf` |

### Cây thư mục

```text
Lab 1/
├── README.md
├── bao_cao/
│   └── SE201613_Lab01_BaoCao.pdf
├── configs/
│   ├── kernel.config
│   └── busybox.config
├── logs/
│   ├── kernel-build.log
│   ├── uboot-build.log
│   ├── partB_uboot.log
│   ├── partC_smp1.log
│   └── partC_smp2.log
├── output/
│   ├── zImage
│   ├── vexpress-v2p-ca9.dtb
│   ├── initramfs.cpio.gz
│   └── u-boot
└── rootfs/
    └── initramfs/
        ├── init
        ├── bin/  sbin/  usr/
        ├── etc/  dev/  proc/  sys/  tmp/
        └── root/
```

Ba file đầu trong `output/`, hai file trong `configs/`, `rootfs/initramfs/` và PDF trong `bao_cao/` là các thành phần bài nộp ở mục V của đề. `output/u-boot` và `logs/` là minh chứng bổ sung cho quá trình thực hành. Mã nguồn đầy đủ của Linux, U-Boot và BusyBox **không** được đưa vào repo; các phiên bản nguồn đã dùng là 5.15, 2022.04 và 1.35.0.

## 2. Môi trường và chuỗi khởi động

| Thành phần | Phiên bản/cấu hình đã dùng |
| --- | --- |
| Host | Ubuntu 26.04.1 LTS, 64-bit |
| Cross-compiler | `arm-linux-gnueabihf-gcc` 15.2.0 |
| QEMU | `qemu-system-arm` 10.2.1 |
| Target | `vexpress-a9`, CPU `cortex-a9`, RAM 512 MiB, `-smp 2` |
| Kernel / U-Boot / BusyBox | Linux 5.15 / U-Boot 2022.04 / BusyBox 1.35.0 |

Luồng khởi động cần hiểu theo đề là **Power ON → ROM bootloader → U-Boot → kernel → init → BusyBox shell**. U-Boot có nhiệm vụ chuẩn bị địa chỉ image, Device Tree và kernel command line; kernel nhận DTB, giải nén initramfs, chạy PID 1; BusyBox init đọc `/etc/inittab`, thực thi `/etc/init.d/rcS` rồi mở shell.

Trong phép thử **Part B**, QEMU chạy `output/u-boot` để kiểm tra bootloader. Trong phép boot **Part C** ở repo này, QEMU nạp trực tiếp `zImage`, DTB và initramfs bằng các tùy chọn `-kernel`, `-dtb`, `-initrd`; đây không phải một lần chain-load kernel từ U-Boot. Sự khác biệt này giúp đối chiếu đúng log với từng phép thử.

## 3. Part A — Cấu hình và build Linux kernel

Nguồn Linux 5.15 được build bên ngoài repo. Từ thư mục nguồn kernel, các bước chính là:

```sh
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
make vexpress_defconfig
make menuconfig
make -j2 KCFLAGS=-march=armv7-a+fp zImage dtbs
```

`vexpress_defconfig` chọn nền tảng Versatile Express. Trong cấu hình đã lưu, các tùy chọn quan trọng là `CONFIG_ARCH_VEXPRESS=y`, `CONFIG_CPU_V7=y`, `CONFIG_BLK_DEV_INITRD=y`, `CONFIG_SERIAL_AMBA_PL011=y`, `CONFIG_SERIAL_AMBA_PL011_CONSOLE=y` và `CONFIG_SMP=y`. Xem [kernel.config](configs/kernel.config) để đối chiếu cấu hình thực tế.

Artifact được lấy từ `arch/arm/boot/zImage` và `arch/arm/boot/dts/vexpress-v2p-ca9.dtb`, rồi đặt vào `output/`. `zImage` là kernel ARM boot image; DTB mô tả phần cứng VExpress CA9 để kernel khởi tạo CPU, bộ nhớ, UART và các thiết bị. Có thể kiểm tra:

```sh
file output/zImage output/vexpress-v2p-ca9.dtb
ls -lh output/zImage output/vexpress-v2p-ca9.dtb
```

Khi build bằng GCC 15 trên host đã dùng, việc ép các cờ CFLAGS/KCFLAGS không phù hợp làm Kbuild rơi về lựa chọn `-march=armv5t`, gây lỗi assembler ở các lệnh ARMv7 như `dmb`, `dsb`, `ldrex`. Bản build thành công dùng `KCFLAGS=-march=armv7-a+fp` như trên; đây là cách xử lý cho môi trường thực hành này, không phải tùy chọn mặc định bắt buộc trên mọi toolchain. [Log build kernel](logs/kernel-build.log) và mục 8 của báo cáo ghi chi tiết quá trình debug.

## 4. Part B — U-Boot và kernel command line

Từ nguồn U-Boot 2022.04, build cho board CA9:

```sh
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
make vexpress_ca9x4_defconfig
make -j"$(nproc)"
```

File ELF `u-boot` sau build được lưu thành `output/u-boot`. Để vào U-Boot shell từ **thư mục `Lab 1/`**:

```sh
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -nographic \
  -kernel output/u-boot
```

Nhấn Enter khi countdown hiện ra. Các lệnh đã thực hành gồm `version`, `bdinfo`, `printenv` và thiết lập các biến sau:

```text
setenv fdt_addr_r 0x60000000
setenv kernel_addr_r 0x60100000
setenv ramdisk_addr_r 0x64000000
setenv bootargs 'console=ttyAMA0,115200 root=/dev/ram0 rw init=/sbin/init mem=512M'
setenv bootcmd 'bootz ${kernel_addr_r} ${ramdisk_addr_r} ${fdt_addr_r}'
printenv fdt_addr_r kernel_addr_r ramdisk_addr_r bootargs bootcmd
```

DRAM của `vexpress-a9` bắt đầu ở vùng `0x60000000`; các địa chỉ kernel, ramdisk và DTB phải thuộc RAM và không chồng lấn. `console=ttyAMA0,115200` chọn UART PL011; `mem=512M` khớp RAM QEMU. [Log U-Boot](logs/partB_uboot.log) có kết quả `printenv` và `setenv` thực tế. Lệnh `saveenv` không lưu bền vững trong cấu hình QEMU/flash đã dùng; báo cáo giải thích lỗi sector boundary.

Phép thử riêng trong Part B boot kernel **chưa có initramfs** để quan sát kernel panic tại bước mount rootfs:

```sh
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -nographic \
  -kernel output/zImage -dtb output/vexpress-v2p-ca9.dtb \
  -append 'console=ttyAMA0,115200 root=/dev/ram0 rw mem=512M'
```

Kernel panic ở phép thử này là kết quả có chủ đích; ảnh và phân tích nằm ở mục 4.5 của báo cáo.

## 5. Part C — BusyBox, rootfs và initramfs

BusyBox 1.35.0 được cross-compile cho ARM và **static link** để rootfs tối giản không cần dynamic loader hoặc shared libraries. Cấu hình lưu trong [busybox.config](configs/busybox.config) có `CONFIG_STATIC=y`, prefix `arm-linux-gnueabihf-` và tắt applet `tc` (`# CONFIG_TC is not set`). Trên host GCC 15, menuconfig của BusyBox 1.35 cần `HOSTCC='gcc -std=gnu89'` để vượt qua phép thử ncurses cũ:

```sh
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
make defconfig
make HOSTCC='gcc -std=gnu89' menuconfig
make -j"$(nproc)"
make install
file _install/bin/busybox
```

`rootfs/initramfs/` chứa BusyBox và các symlink applet trong `bin/`, `sbin/`, `usr/`. File `/init` gọi `/sbin/init`; `/etc/inittab` chạy `/etc/init.d/rcS`; script `rcS` mount `proc`, `sysfs`, `tmpfs`, đặt hostname `embedded-lab` và in thông báo hoàn tất. `dev/`, `proc/`, `sys/`, `tmp/` là các đường dẫn cần thiết cho userspace.

**Lưu ý khi clone Git:** Git không lưu Linux character device node. Bản cây thư mục trên GitHub có [hướng dẫn trong `dev/`](rootfs/initramfs/dev/README.md), còn `output/initramfs.cpio.gz` đã chứa đủ `dev/console`, `dev/null`, `dev/zero`, `dev/random` với kiểu device node và quyền tương ứng. Vì vậy hãy dùng file cpio đã build để boot. Nếu cần đóng gói lại từ cây đã clone, tạo lại các node trước theo hướng dẫn đó; các file `.gitkeep` chỉ giúp Git giữ thư mục rỗng.

Lệnh tạo cpio trong quá trình thực hành, chạy từ bên trong cây `rootfs/initramfs/` sau khi đã có device nodes:

```sh
find . | cpio -H newc -o --owner=0:0 | gzip -9 > ../../output/initramfs.cpio.gz
```

Kiểm tra archive mà không giải nén:

```sh
zcat output/initramfs.cpio.gz | cpio -t | head -30
zcat output/initramfs.cpio.gz | cpio -itv | grep -E 'dev/(console|null|zero|random)$'
```

### Boot hệ thống hoàn chỉnh

Từ **thư mục `Lab 1/`**, chạy:

```sh
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel output/zImage \
  -dtb output/vexpress-v2p-ca9.dtb \
  -initrd output/initramfs.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'
```

Sau dòng `Boot completed! Welcome to Embedded Linux Lab 1`, nhấn Enter để vào BusyBox shell. Kiểm tra trong máy ảo:

```sh
uname -a
cat /proc/cpuinfo
cat /proc/meminfo | head -5
mount
ps
cat /sys/devices/system/cpu/online
```

[Log `-smp 2`](logs/partC_smp2.log) cho thấy kernel nhận hai CPU ARMv7 (`CPU part: 0xc09`, Cortex-A9), `smp: Brought up 1 node, 2 CPUs`, mount rootfs/proc/sysfs/tmpfs và chạy PID 1. [Log `-smp 1`](logs/partC_smp1.log) cho thấy một CPU; hai log phục vụ câu hỏi so sánh SMP trong đề. Thoát QEMU bằng `Ctrl+A`, rồi `X`.

## 6. Báo cáo, minh chứng và câu hỏi phân tích

[Báo cáo PDF duy nhất](bao_cao/SE201613_Lab01_BaoCao.pdf) gồm thông tin nhóm; Boot Sequence Diagram; minh chứng Part A, B, C; kết quả `uname`, `cpuinfo`, `meminfo`, `mount`, `ps`; 5 câu hỏi phân tích; và các lỗi cùng cách xử lý. Các câu hỏi cuối đề tập trung vào:

1. Khác biệt ABI/FPU giữa `gnueabi` và `gnueabihf`.
2. Memory map `vexpress-a9` và địa chỉ load kernel/DTB/initramfs.
3. MIDR `0x410fc090`, trong đó part number `0xc09` là Cortex-A9.
4. Lý do chọn BusyBox static và thư viện cần thiết nếu build dynamic.
5. Khác biệt boot log và khởi tạo CPU giữa `-smp 1` và `-smp 2`.

Các log gốc trong `logs/` giúp đối chiếu với minh chứng của báo cáo. README tóm tắt quy trình; PDF chứa phần giải thích kỹ thuật và ảnh chụp đầy đủ.

## 7. Các sự cố đã gặp

| Sự cố | Nguyên nhân/cách xử lý trong bài thực hành |
| --- | --- |
| Kernel assembler báo `dmb`/`dsb`/`ldrex` không được hỗ trợ | Kiểm tra compile command, bỏ cờ môi trường xung đột và build với `KCFLAGS=-march=armv7-a+fp` trên GCC 15. |
| BusyBox `menuconfig` báo thiếu ncurses | Phép thử host cũ không tương thích GCC 15; dùng `make HOSTCC='gcc -std=gnu89' menuconfig`. |
| BusyBox applet `tc` lỗi với Linux UAPI headers | Tắt `CONFIG_TC` vì applet này không cần cho initramfs tối giản. |
| U-Boot `saveenv` lỗi sector boundary | Dùng `setenv` trong phiên QEMU; muốn lưu bền vững cần sửa vị trí environment trong cấu hình flash và build lại. |

Chi tiết triệu chứng, nguyên nhân và bằng chứng xác minh nằm ở mục 8 của báo cáo.
