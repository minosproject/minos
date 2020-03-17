# Test Minos on ARM FVP

Minos uses ARM FVP (Fixed Virtual Platform) for debugging. It has test on latest version of ARM FVP(installed in DS-5_v5.29.3). This guide is based on the DS-5.

You can follow the [ARM FVP Linux Kernel Debugging Concise Manual](https://www.jianshu.com/p/c0a9a4b9569d) for the first view of DS-5 and ARM FVP. Then following the instructions below to debug minos.

## 1. Create a working directory

```shell script
mkdir ~/minos-workspace/arm-fvp
```

## 2. Build Minos

   ```shell script
   cd ~/minos-workspace
   git clone https://github.com/minosproject/minos.git
   cd minos
   make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu- fvp_defconfig
   make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu- O=0
   make dtbs
   ```

   `O=0` means close the O2 optimization for debug step in.

## 3. Build FVP Kernel

   ``` shell script
   cd ~/minos-workspace
   git clone https://github.com/minosproject/linux-marvell.git
   cd linux-marvell
   make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig
   make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8 Image
   ```

## 4. Compile ARM Trusted Firmware

   ```shell script
   cd ~/minos-workspace
   git clone https://github.com/ARM-software/arm-trusted-firmware.git
   cd arm-trusted-firmware
   make CROSS_COMPILE=aarch64-linux-gnu- PLAT=fvp RESET_TO_BL31=1 ARM_LINUX_KERNEL_AS_BL33=1 PRELOADED_BL33_BASE=0xc0008000 ARM_PRELOADED_DTB_BASE=0xc3e00000
   ```

   That will build the `bl31.bin` in `arm-trusted-firmware/build/fvp/release/`. ARM FVP using `bl31.bin` to do something like bootloader(u-boot) to boot minos.

## 5. Build ARM64 virtio-block image

   ```shell script
   cd ~/minos-workspace
   wget https://releases.linaro.org/archive/14.07/openembedded/aarch64/vexpress64-openembedded_minimal-armv8-gcc-4.9_20140727-682.img.gz
   gunzip vexpress64-openembedded_minimal-armv8-gcc-4.9_20140727-682.img.gz
   mv vexpress64-openembedded_minimal-armv8-gcc-4.9_20140727-682.img sd.img
   ```

## 6. Get other images

  ```shell script
  git clone https://github.com/minosproject/minos-misc.git
  ```

  In this repository includes some other built images and dtb.

## 7. Collecting everything ARM FVP needed

  ```shell script
  cd ~/minos-workspace/arm-fvp
  ln -s ~/minos-workspace/sd.img sd.img
  ln -s ~/minos-workspace/arm-trusted-firmware/build/fvp/release/bl31.bin bl31.bin
  ln -s ~/minos-workspace/linux-marvell/arch/arm64/boot/Image Image
  ln -s ~/minos-workspace/minos/dtbs/foundation-v8-gicv3.dtb fdt.dtb
  ln -s ~/minos-workspace/minos-hypervisor/hypervisor/minos.bin minos.bin
  ln -s ~/minos-workspace/minos-misc/arm-fvp/fvp_linux.dtb fvp_linux.dtb
  ```
## 8. Boot minos on DS-5

- Setup DS-5

  Enter Run->debug configuration, in Connection tab select taget: ARM FVP(installed with DS-5)/Base_AEMv8Ax4/Bare Metal Debug/Debug ARMAEMv8-A\_x4 SMP.

  Model parameters showing below:

  ```shell script
  -C pctl.startup=0.0.0.0 \
  -C bp.secure_memory=0 \
  -C cluster0.NUM_CORES=4 \
  -C cache_state_modelled=0 \
  -C cluster0.cpu0.RVBAR=0x04020000 \
  -C cluster0.cpu1.RVBAR=0x04020000 \
  -C cluster0.cpu2.RVBAR=0x04020000 \
  -C cluster0.cpu3.RVBAR=0x04020000 \
  -C bp.hostbridge.userNetPorts="8023=22" \
  -C bp.hostbridge.userNetworking=true \
  -C bp.dram_size=8 \
  -C bp.smsc_91c111.enabled=true \
  -C bp.virtioblockdevice.image_path=/home/{whoami}/minos-workspace/arm-fvp/sd.img \
  --data cluster0.cpu0=/home/{whoami}/minos-workspace/arm-fvp/bl31.bin@0x04020000 \
  --data cluster0.cpu0=/home/{whoami}/minos-workspace/arm-fvp/Image@0x80080000 \
  --data cluster0.cpu0=/home/{whoami}/minos-workspace/arm-fvp/minos.bin@0xc0008000 \
  --data cluster0.cpu0=/home/{whoami}/minos-workspace/arm-fvp/fdt.dtb@0xc3e00000
  --data cluster0.cpu0=/home/{whoami}/minos-workspace/arm-fvp/fvp_linux.dtb@0x83e00000
  ```
  At debugger tab, select Run control as Connect only. Select Execute debug commands and input command showing below:

  ```shell script
  add-symbol-file minos.elf EL2:0x0
  ```
  You need to make sure that the `minos.elf` is in your DS-5 workspace. Then, set the Source Path as minos location.

- Start debug

  When you start debug, firstly, the DS-5 will stop at arm-trusted-firmware entry point(in EL3). You need set breakpoint at `_start` to entry minos. Using command `b EL2N:_start` to do that. The prefix is needed in the first time to entry the minos code(in EL2).

  Then enjoy it!
