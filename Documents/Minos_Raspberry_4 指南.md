# Minos - Raspberry 4 指南

Minos默认使用raspberry4进行开发和测试，RPI4有4个A72核以及集成了GICV2，终端控制器的使用GICV2在虚拟化的支持上要比RPI3要更好，以下文档说明了如何在RPI4上部署Minos，启动两个Native虚拟机.

## 编译和运行VM0

#### 下载代码

- 制作sd启动image, 参考官方指南

- 下载u-boot代码

  ```
  git clone ssh://git@192.168.50.179:30001/minle/rpi-uboot.git
  ```

- 下载linux kernel代码

  ```
  git clone ssh://git@192.168.50.179:30001/minle/linux-raspberry.git
  ```

- 下载Minos代码

  ```
  git clone ssh://git@192.168.50.179:30001/minle/minos.git
  ```

- 下载minos-sample

  ```
  git clone ssh://git@192.168.50.179:30001/minle/minos-sample.git
  ```

#### 编译代码

- 编译u-boot (需要编译64位 u-boot，32位没有测试)

  ```
  make CROSS_COMPILE=aarch64-linux-gnu- rpi_4_defconfig
  make CROSS_COMPILE=aarch64-linux-gnu- -j8
  ```

- 编译Kernel以及modules

  - 32位内核

  ```
  make ARCH=arm CROSS_COMPILE=armv8l-linux-gnueabihf- bcm2711_defconfig
  make ARCH=arm CROSS_COMPILE=armv8l-linux-gnueabihf- Image -j24
  make ARCH=arm CROSS_COMPILE=armv8l-linux-gnueabihf- modules dtbs -j24
  ```

  - 64位内核

  ```
  make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- bcm2711_defconfig
  make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image -j24
  make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules dtbs -j24
  ```

- 编译Minos

  ```
  make rpi_4_defconfig
  make && make dtbs
  ```

- 编译 VM0 DTS (minos-samples)

  ```
  dtc -I dts -O dtb -o rpi_4.dtb bcm2711-rpi-4-b.dts
  ```

#### 更新SD卡镜像

Minos当前已经可以同时支持32位和64位的VM，且支持官方的相关镜像，请参考官方文档把相关镜像烧写到SD卡中，然后再进行以下步骤，官方链接如下(https://www.raspberrypi.org/documentation/installation/installing-images/README.md)，假设SD卡boot分区挂载在 /media/minle/boot，根文件系统分区挂载在/media/minle/rootfs下

- u-boot

  ```
  cp u-boot.bin /media/minle/boot/kernel8.img
  ```

- 修改config.txt，修改后内容如下

  ```
  disable_overscan=1
  dtparam=audio=on

  [pi4]
  dtoverlay=vc4-fkms-v3d
  max_framebuffers=2
  arm_64bit=1
  enable_uart=1
  ```

- 拷贝VM0 kernel及dts

  ```
  cp arch/arm/boot/Image /media/minle/boot
  cp rpi_4.dtb /media/minle/boot
  ```

- 更新系统的modules

  ```
  sudo make ARCH=arm CROSS_COMPILE=armv8l-linux-gnueabihf- modules_install INSTALL_MOD_PATH=/media/minle/rootfs
  ```

#### 启动系统

运行系统两种方式:

1. 手动模式，手动加载image到对应内存
2. 自动模式，把加载镜像的命令编译进u-boot，u-boot启动的时候自动加载相关镜像

这里只讲解手动模式，命令如下

```
fatload mmc 0:1 0x37408000 minos.bin
fatload mmc 0:1 0x38000000 minos.dtb (位于minos代码下 dtbs/bcm2838-rpi-4-b-32bit.dtb)
fatload mmc 0:1 0x00008000 Image
fatload mmc 0:1 0x03e00000 rpi_4.dtb
booti 0x37408000 - 0x38000000 (启动VM0)
```

## 编译和运行VM1

Minos支持两种方式启动VM，静态方式和动态方式，静态方式是指此VM相关image会由bootloader加载到内存中，且此VM的所有内存在启动前就已经分配好(一段连续的物理内存)，然后VM直接在Hypervisor启动后和VM0一起启动，无需等待VM0启动完在启动，这种VM也被称作**Native VM**。动态方式是指此VM通过运行在VM0里面的mvm程序动态创建的，此种VM也被称作**Guest VM**。

- 编译VM1 Kernel

  默认如果都是Linux系统，所有镜像都可以和VM0的Kernel镜像使用同一个Image，但是当前还没有在默认镜像中使能ramdisk功能，所以以arm fvp的Kernel镜像作为VM1的镜像

  ```
  make ARCH=arm CROSS_COMPILE=armv8l-linux-gnueabihf- vexpress_defconfig
  make ARCH=arm CROSS_COMPILE=armv8l-linux-gnueabihf- Image -j24
  ```

  DTB和ramdisk镜像可以在minos-sample/raspberry/rpi-4/native_vm_32bit中找到，假设这三个image的名字分别位

  ```
  vm1_kernel.img
  vm1_dtb.img
  vm1_ramdisk.img
  ```

- 同时启动VM0和VM1

  ```
  fatload mmc 0:1 0x37408000 minos.bin
  fatload mmc 0:1 0x38000000 minos.dtb (位于minos代码下 dtbs/bcm2838-rpi-4-b-32bit.dtb)
  fatload mmc 0:1 0x00008000 Image
  fatload mmc 0:1 0x03e00000 rpi_4.dtb
  fatload mmc 0:1 0x30008000 vm1_kernel.img
  fatload mmc 0:1 0x33e00000 vm1_dtb.img
  fatload mmc 0:1 0x34000000 vm1_ramdisk.img
  ```


