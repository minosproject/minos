# Build Minos

### Download Source Code And Tools for Minos

1. Create a working directory

       # mkdir ~/minos-workspace
       # cd ~/minos-workspace

2. Install aarch64 gcc cross compilation tool

       # wget https://releases.linaro.org/components/toolchain/binaries/7.2-2017.11/aarch64-linux-gnu/gcc-linaro-7.2.1-2017.11-x86_64_aarch64-linux-gnu.tar.xz
       # tar xjf gcc-linaro-7.2.1-2017.11-x86_64_aarch64-linux-gnu.tar.xz
       # sudo mv gcc-linaro-7.2.1-2017.11-x86_64_aarch64-linux-gnu /opt
       # echo "export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_aarch64-linux-gnu/bin:$PATH" >> ~/.bashrc
       # source ~/.bashrc

3. Install abootimg android image tool

       # sudo apt-get install abootimg

   The abootimg tool is used to make the bootimge of the Linux VM. mvm uses this format image to load the linux kernel, ramdisk and dtb files.

4. Install device-tree tool

       # sudo apt-get install device-tree-compiler

6. Download Minos source code

       # git clone https://github.com/minosproject/minos-hypervisor.git

### Compile Minos

|    Board    |      Config File      |           dts file           |
| :---------: | :-------------------: | :--------------------------: |
| espressobin | espressobin_defconfig | armada-3720-community-v5.dts |
|   arm-fvp   |     fvp_defconfig     |   foundation-v8-gicv3.dts    |
|    VIM3     |    kvim3_defconfig    |          kvim3.dts           |
|   RPI-3B    |    rpi_3_defconfig    |   bcm2837-rpi-3-b-plus.dts   |
|    RPI-4    |    rpi_4_defconfig    |  bcm2838-rpi-4-b-32bit.dts   |

```
# make xxx_defconfig (xxx_defconfig see above table)
# make menuconfig (option)
# make
# make mvm
# make mkrmd
```

#### images generated

| Name              | Comments                                 |
| ------------------| -----------------------------------------|
| minos.bin         | binary image of  minos                   |
| dtbs/xxx.dtb      | device tree image for each board         |
| tools/mvm/mvm     | user space tools to create guest VM      |
| tools/mkrmd/mkrmd | user space tools to create ramdisk image |
