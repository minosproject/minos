# Test Minos on Raspberry 3B

Minos has been tested on Raspberry Pi 3 Model b+, Raspberry Pi 3 and 3 Model A+ are supported too. These boards use Broadcom's bcm28737 chip, which does not use GICv2 or GICv3 interrupt controllers that support interrupt virtualization. In order to implement interrupt virtualization on this chip and minimize VM code modifications, the following method was adopted:

> * Implement virtual bcm2836-armctrl-ic and bcm2836-l1-intc interrupt controller for Host VM (VM0)
> * Extended the vGICv2 for the Guest VM

I just test 64bit Kernel for RPI-3, but I think 32bit  kernel is also ok.

## Make SD card image

You can use any raspberry system image to make the SD card image, I only test 64bit kernel + 64bit userspace,  but 32bit user-space may also ok. I used below image for test:

```
   Link: https://pan.baidu.com/s/17DNKYIIiAodf6aTGct3-Cw passwd: aeg7
   
   system username : jiangxianxu passwd:linux
```

you can modify **/etc/wpa_supplicant/wpa_supplicant.conf**  to connect the network

```
jiangxianxu@debianOnRpi:~$ cat /etc/wpa_supplicant/wpa_supplicant.conf 
#ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
update_config=1
country=CN
 
network={
        ssid="your ssid"
        psk="your passwd"
}
```

then use below command to ssh to the rpi3b

```
ssh -p 1314 jiangxianxu@xxx.xxx.xxx.xxx
```

Plug in the SD card to Ubuntu system again, the two partition will be mounted automaticlly, here on my side, **the boot partition will be mount at /media/minle/6A99-E637, and the rootfs partition will be mounted at /media/minle/711a5ddf-1ff4-4d5a-ad95-8b9b69953513, the system used may not be the same. The following commands which modify the file on theses partitions, need to adjust the path according to the actual situation**.

## Download code and build images

1. ### Build Minos

   ```
   # git clone https://github.com/minosproject/minos-hypervisor.git
   # cd minos
   # make rpi_3_defconfig
   # make && make mvm
   ```

2. Build u-boot

   ```
   # git clone https://github.com/u-boot/u-boot.git && cd u-boot
   # export CROSS_COMPILE=aarch64-linux-gnu-
   # make rpi_3_defconfig
   # make -j8
   ```

3. Build Kernel and Kernel modules

   ```
   The Minos kernel driver is under ${MINOS_SRC}/generic/minos-linux-driver, copy or link this folder to your kernel source tree and build this driver to kernel. Currently the kernel driver has been test on 4.19.238 and 4.4.52.

   # make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- bcmrpi3_defconfig
   # make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image -j8
   # make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules dtbs -j8
   # sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules_install INSTALL_MOD_PATH=/media/zac/711a5ddf-1ff4-4d5a-ad95-8b9b69953513
   # cp arch/arm64/boot/Image /media/minle/6A99-E637
   ```

4. Get other images

   ```
   # git clone https://github.com/minosproject/minos-misc.git
   ```

## Update SD card image

- copy images

  ```
  # cp u-boot/u-boot.bin /media/minle/6A99-E637/kernel8.img
  # cp minos/minos.bin /media/minle/6A99-E637
  # cp minos/dtbs/bcm2837-rpi-3-b-plus.dtb /media/minle/6A99-E637/minos.dtb
  # cp minos-misc/rpi-3/vm0.dtb /media/minle/6A99-E637/
  ```

- change the /media/minle/boo/config.txt to below content

  ```
  ￼arm_control=0x200
  #dtoverlay=pi3-miniuart-bt
  enable_uart=1
  kernel=kernel8.img
  ```
  

## Boot System

Connect your raspberry4 with a usb serial, and enter into u-boot command line mode,

```
fatload mmc 0:1 0x28008000 minos.bin;
fatload mmc 0:1 0x29e00000 minos.dtb;
fatload mmc 0:1 0x80000 Image; 
fatload mmc 0:1 0x03e00000 vm0.dtb; 
booti 0x28008000 - 0x29e00000
```

You can write below command to u-boot's common/Kconfig, then u-boot will auto load these image and jump to Minos (need make clean and recompile u-boot)

```
diff --git a/common/Kconfig b/common/Kconfig
index 46e4193fc8..67c8771c1b 100644
--- a/common/Kconfig
+++ b/common/Kconfig
@@ -396,7 +396,7 @@ config USE_BOOTCOMMAND
 config BOOTCOMMAND
        string "bootcmd value"
        depends on USE_BOOTCOMMAND
-       default "run distro_bootcmd" if DISTRO_DEFAULTS
+       default "fatload mmc 0:1 0x28008000 minos.bin; fatload mmc 0:1 0x29e00000 minos.dtb; fatload mmc 0:1 0x80000 Image; fatload mmc 0:1 0x03e00000 vm0.dtb; booti 0x28008000 - 0x29e00000" if DISTRO_DEFAULTS
        help
          This is the string of commands that will be used as bootcmd and if
          AUTOBOOT is set, automatically run.
```

## Create Guest VM using MVM

copy below images to your rpi3 system

1. mvm - minos/tools/mvm/mvm
2. aarch32-boot.img  - minos-misc
3. aarch64-boot.img - minos-misc

use below command to create a 64bit Guest VM

```
sudo ./mvm run_as_daemon memory=64M vm_name=linux vm_os=linux vcpus=2 os-64bit bootimage=aarch64-boot.img cmdline="console=hvc0 loglevel=8 consolelog=9" gic=gicv2 device@virtio-console,backend=@pty
```

and below command can create a 32bit Guest VM with the virtioblk rootfs

```
sudo ./mvm run_as_daemon memory=64M vm_name=linux vm_os=linux vcpus=2 bootimage=boot32.img no-ramdisk cmdline="console=hvc0 loglevel=8 consolelog=9 root=/dev/vda2 rw" gic=gicv2 device@virtio-console,backend=@pty device@virtio_blk,/virtio_image_path/xxx.img
```

