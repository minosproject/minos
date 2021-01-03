# Test Minos on Raspberry 4

Minos both support 64bit and 32bit kernel for raspberry4, here we will use 32bit raspbian as the VM0, and use `dtbs/bcm2838-rpi-4-b-32bit.dts`  as the dtb file for system, in this file, will created 3 VMs

**VM0** - raspbian

**VM1** - 32bit linux

**VM2** - 64bit zephyr

This is only for raspberry-4b 4GB version, other please modify the config.

## Download code and build images

1. ### Build Minos

   ```
   # git clone https://github.com/minosproject/minos-hypervisor.git
   # cd minos
   # make rpi_4_defconfig
   # make && make dtbs && make mvm
   ```

2. Build u-boot

   ```
   # git clone https://github.com/agherzan/u-boot.git
   # cd u-boot
   # make CROSS_COMPILE=aarch64-linux-gnu- rpi_4_defconfig
   # make CROSS_COMPILE=aarch64-linux-gnu- -j8
   ```

3. Build Kernel and Kernel modules

   ```
   git clone https://github.com/minosproject/linux-raspberry.git
   git checkout -b minos-rpi4 origin/minos-rpi4
   ```

   - 32bit Kernel

     ```
     # cd linux-raspberry
     # make ARCH=arm CROSS_COMPILE=armv8l-linux-gnueabihf- bcm2711_defconfig
     # make ARCH=arm CROSS_COMPILE=armv8l-linux-gnueabihf- Image -j24
     # make ARCH=arm CROSS_COMPILE=armv8l-linux-gnueabihf- modules dtbs -j24
     ```

   - 64bit kernel

     ```
     # cd linux-raspberry
     # make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- bcm2711_defconfig
     # make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image -j24
     # make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules dtbs -j24
     ```

4. Get other images

   ```
   # git clone https://github.com/minosproject/minos-misc.git
   ```

## Make SD card image

Minos currently supports both 32-bit and 64-bit VMs, and supports official related images. Please refer to the official documentation to flash the related images to the SD card, and then perform the following steps. The official link is as follows (https://www.raspberrypi.org/documentation/installation/installing-images/README.md), assuming the SD card boot partition is mounted at /media/minle/boot and the root file system partition is mounted at /media/minle/rootfs

## Update SD card image

- u-boot

  ```
  # cd u-boot
  # cp u-boot.bin /media/minle/boot/kernel8.img
  ```

- change the /media/minle/boo/config.txt to below content

  ```
  disable_overscan=1
  dtparam=audio=on
  
  [pi4]
  dtoverlay=vc4-fkms-v3d
  max_framebuffers=2
  arm_64bit=1
  enable_uart=1
  ```

- copy VM0 kernel Image and dtb file

  ```
  # cd linux-raspberry
  # cp arch/arm/boot/Image /media/minle/boot
  # cd minos-misc
  # cp vm0_dtb.img /media/minle/boot
  ```

- update Kernel modules

  ```
  # cd linux-raspberry
  # sudo make ARCH=arm CROSS_COMPILE=armv8l-linux-gnueabihf- modules_install INSTALL_MOD_PATH=/media/minle/rootfs
  ```

- copy zephyr image

  ```
  # cd minos-misc
  # cp zephyr.bin /media/minle/boot
  ```

- copy VM1 ramdisk image and dtb image

  ```
  # cp minos-misc
  # cp vm1_ramdisk.img /media/minle/boot
  # cp vm1_dtb.img /media/minle/boot
  ```

## Boot System

Connect your raspberry4 with a usb serial, and enter into u-boot command line mode

```
# fatload mmc 0:1 0x37200000 zephyr.bin; fatload mmc 0:1 0x30008000 Image; fatload mmc 0:1 0x33e00000 vm1_dtb.img; fatload mmc 0:1 0x34000000 vm1_ramdisk.img; fatload mmc 0:1 0x37408000 minos.bin; fatload mmc  0:1 0x38000000 minos.dtb; fatload mmc 0:1 0x00080000 Image; fatload mmc 0:1 0x03e00000 vm0_dtb.img; booti 0x37408000 - 0x38000000
```

 More information, please see below video:

https://www.youtube.com/watch?v=00r66CUTh1E

or

https://v.qq.com/x/page/l30352185ek.html
