Git không lưu được Linux character device nodes. Bốn node của rootfs gốc
(`console`, `null`, `zero`, `random`) đã được giữ đúng kiểu và quyền trong
`Lab 1/output/initramfs.cpio.gz`. Bản cpio đó có thể dùng trực tiếp để boot.

Nếu cần tạo lại cpio từ cây thư mục sau khi clone repo, chạy từ `Lab 1/rootfs/initramfs`:

```sh
sudo mknod -m 600 dev/console c 5 1
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/zero c 1 5
sudo mknod -m 444 dev/random c 1 8
find . | cpio -H newc -o --owner=0:0 | gzip -9 > ../../output/initramfs.cpio.gz
```

Kiểm tra bốn node trong cpio:

```sh
zcat ../../output/initramfs.cpio.gz | cpio -itv | grep -E 'dev/(console|null|zero|random)$'
```
