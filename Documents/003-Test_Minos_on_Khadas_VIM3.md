# Test Minos on Khadas VIM3 Pro

Khadas VIM3 Pro is official supported by Minos, the following will introduce how to start three VMs on VIM3-Pro, the 3 VM3 including Android, Linux, Zephyr. Only tested on VIM3 Pro, VIM3 needs to adjust the memory layout

## 1. Create a working directory

```shell script
mkdir ~/minos-vim3
```

## 2. Build Minos

   ```shell script
   cd ~/minos-vim3
   git clone https://github.com/minosproject/minos-hypervisor.git
   cd minos-hypervisor
   make kvim3_defconfig
   make
   make dtbs
   ```

## 3. Setup Serial Debugging Tool

Please refers to Khadas office document https://docs.khadas.com/vim3/SetupSerialTool.html

## 4. Setup TFTP for VIM3

Please refers to Khadas office document https://docs.khadas.com/vim3/SetupTFTPServer.html

## 5. Get other images

We will run 3 VMs simultaneously, the Android will in the VIM3 emmc, but will use a different kernel Image for Android which has been integrated with the patches from Minos. Other two VM are Linux (64 bit) and Zephyr (64 bit), please download the below git repository:

  ```shell script
git clone https://github.com/minosproject/minos-misc.git
  ```

  In this repository includes all the built images and dtbs.

## 7. Collecting everything ARM FVP needed

  ```shell script
cd ~/minos-vim3
cp minos-hypervisor/minos.bin /var/lib/tftpboot/minos.bin
cp minos-hypervisor/dtbs/kvim3.dtb /var/lib/tftpboot/minos.dtb
cd minos-misc/khadas-vim3-pro
cp vm0_dtb.img vm0_Image vm1_dtb.dts vm1_dtb.img vm1_Image vm1_ramdisk.img zephyr.bin /var/lib/tftpboot/
  ```

## 8. Boot Minos and VM3 on VIM3 Pro

To setup TFTP on your target device, you will need to:

- Connect a LAN cable to your target device, and make sure your device is on same local network with your Host PC.
- Connect a “Serial-To-USB Module” between the target device and Host PC and ensure you have done the correct [setup](https://docs.khadas.com/vim1/SetupSerialTool.html).
- Power-on your target device, and ensure the device has a Bootloader installed in it.

Stop U-Boot autoboot by hitting `Enter` or `Space` key at the moment you power on your target device:

```
U-Boot 2015.01 (May 18 2019 - 19:31:53)

DRAM:  3.8 GiB
Relocation Offset is: d6e56000

...

gpio: pin GPIOAO_7 (gpio 7) value is 1
Hit Enter or space or Ctrl+C key to stop autoboot -- :  0 
kvim3#
```

```shell script
tftp eb80c000 minos.bin
tftp ed600000 minos.dtb
tftp 10000000 vm0_dtb.img
tftp 0x108000 vm0_Image
tftp 0x80080000 vm1_Image
tftp 0x83e00000 vm1_dtb.img
tftp 0x84000000 vm1_ramdisk.img
tftp 0x88000000 zephyr.bin
booti 0xeb80c000 - 0x10000000
```

## 9. Switch different VM console

As default, Minos will attach the vm0's debug console with below dts setting in dtbs/kvims.dts

```
        chosen {
                minos,stdout = "aml_meson";
                bootargs = "bootwait=3 tty=vm0";
        };
```

 you can press `ctrl + d` to exit the debug console from one vm, then enter below command to attach other VM's console

```
minos # tty attach vm1
```

