# Run Minos on Qemu

Create a folder as the working directory.

```bash
# mkdir ~/minos-qemu
# cd ~minos-qemu
# sudo apt install qemu-system-aarch64
```

## Build U-boot

```bash
# git clone https://github.com/minosproject/u-boot.git
# cd u-boot
# make qemu_arm64_defconfig
# make -j16 CROSS_COMPILE=aarch64-linux-gnu-
```

u-boot.bin will be used as the bootloader for minos.

## Build Minos

We need at least three files generated from minos

1. minos/minos.bin  (The hypervisor binary)
2. minos/dtbs/qemu-arm64.dtb (The device tree of hypervisor)
3. tools/mkrmd/mkrmd (Used to generate ramdisk of minos)

### Build minos hypervisor

```bash
# git clone https://github.com/minosproject/minos.git
# cd minos
# make qemu_arm64_defconfig
# make
```

### Build ramdisk tools

```bash
# cd tools/mkrmd
# make
```

After above steps the three files will be generated.

## Build LInux Kernel

```bash
# wget https://mirrors.edge.kernel.org/pub/linux/kernel/v4.x/linux-4.19.238.tar.gz
# tar xvzf linux-4.19.238.tar.gz
# cp -r minos/generic/minos-linux-driver linux-4.19.238/drivers/minos
# cd linux-4.19.238
```

Add below content to drivers/Makefile

```makefile
obj-y                           += minos/
```

Then Build the host VM kernel

```bash
# make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig
# make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8 Image
```

## Create the ramdisk for minos

Dowload the host vm dtb file from:

```
链接: https://pan.baidu.com/s/1qA9QiHxM7cdwITdUyl0Bfg?pwd=pu9b 提取码: pu9b 复制这段内容后打开百度网盘手机App，操作更方便哦
```

Create the ramdisk

```bash
# ./minos/tools/mkrmd/mkrmd -f ramdisk.bin linux-4.19.238/arch/arm64/boot/Image ./qemu-virt.dtb
```

The output log when mkrmd will be like this

```
open or create target file [ramdisk.bin]
create ramdisk using FILE mode
Image f_offset 0x0 0x19042816
qemu-virt.dtb f_offset 0x19046400 0x6899
generate ramdisk ramdisk.bin done
file_cnt 2 inode_offset 0x30 data_offset 0x1000
ramdisk size 0x122d000
```

## Prepare the rootfs image

A simple rootfs can be downloaded at pan.baidu.com or create it by youself.

```
512M version: 链接: https://pan.baidu.com/s/1Lmy_UBtaLw1IXcz9FbieaA 提取码: si73
3G version  : 链接: https://pan.baidu.com/s/1reEhw5ct3yXnDzPKudsisg?pwd=it3x 提取码: it3x
```

Then mount the boot partition (fat32) of the image and copy all needed files into the partition. （https://unix.stackexchange.com/questions/82314/how-to-find-the-type-of-an-img-file-and-mount-it   how to mount the virtio image file）

```bash
# mkdir sdboot
# sudo mount -o loop,offset=32256 virtio-sd.img sdboot
# cp minos/minos.bin sdboot/kernel.bin
# cp minos/dtbs/qemu-arm64.dtb sdboot/
# cp ramdisk.bin sdboot/
```

## Boot the system

```bash
qemu-system-aarch64 -nographic -bios u-boot/u-boot.bin \
	-M virtualization=on,gic-version=3 \
	-cpu cortex-a53 -machine type=virt -smp 4 -m 2G -machine virtualization=true \
	-drive if=none,file=virtio-sd.img,format=raw,id=hd0 \
	-device virtio-blk-device,drive=hd0 \
	-device virtio-net-device,netdev=net0 -netdev user,id=net0,hostfwd=tcp:127.0.0.1:5555-:22
```

**For how to create virtual machines, you can refers to other documents.**

