Git không lưu được Linux character device nodes. Bốn node của bản rootfs gốc
(`console`, `null`, `zero`, `random`) được giữ nguyên trong file `.tar.gz` và
trong `output/initramfs.cpio.gz` ở thư mục Lab01.

Nếu cần tạo lại sau khi clone repo, chạy từ thư mục `rootfs/initramfs`:

```sh
sudo mknod -m 600 dev/console c 5 1
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/zero c 1 5
sudo mknod -m 444 dev/random c 1 8
```
