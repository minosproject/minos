# Minos - Type 1 Hypervisor for ARMv8-A

Minos is a lightweight open source Type 1 Hypervisor for mobile and embedded systems that runs directly in bare metal environments. Minos implements a complete virtualization framework that can run multiple VMs (Linux or RTOS) on one hardware platform. Minos provides CPU virtualization; interrupt virtualization; memory virtualization; Timer virtual; and the virtualization of some common peripherals.

Minos provides an application "mvm" running on VM0 to support the management of the Guest VM. At the same time, mvm provides a virtio-based paravirtualization solution that supports virtio-console, virtio-blk, virtio-net and other devices. Minos can support both 64bit and 32bit guest VM, but VM0 must use 64bit.

Minos is suitable for mobile and embedded platforms and currently only supports the ARMv8-A architecture. Marvell's Esspressobin and Raspberry Pi 3 are supported, and the hardware platform of the ARMv8-A + GICV3/GICV2 combination can theoretically be supported. The software debugging platform supports ARM's official Fix Virtual Platform (FVP), and developers can use ARM DS5 tools for simulation and debugging.

Below is the board that Minos has been supported:

- [x] Marvell's Esspressobin development board
- [x] Raspberry Pi 3 Model B+ / Raspberry Pi 3 Model A+ / Raspberry Pi 3 Model B
- [x] ARMv8 Fixed Virtual Platforms


# Download Source Code And Tools for Minos

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

4. Install devicetree tool

        # sudo apt-get install device-tree-compiler

5. Download Minos sample

        # git clone https://github.com/minos-project/minos-samples.git
        # make

	The minos-sample provides the dts/dtb file of the Guest VM and the created Guest VM boot.img file.
        - aarch32-boot.img - boot image for aarch32 VM contains the ramdisk and dtb file
        - aarch64-boot.img - boot image　for aarch64 VM contains the ramdisk and dtb file

6. Download Minos hypervisor source code

        # git clone https://github.com/minos-project/minos-hypervisor.git

7. Download Linux Kernel source code

        # git clone https://github.com/minos-project/linux-marvell.git
        # cd linux-marvell
        # git checkout -b minos origin/minos

	The default download is the Marvell linux kernel source which added the Minos kernel driver. If you are using another hardware platform, just add the Minos driver. The following command can get the Minos driver and the necessary Kernel Patch.

        # git clone  https://github.com/minos-project/minos-linux-driver.git

8. Download the ATF source code

        # git clone https://github.com/ARM-software/arm-trusted-firmware.git

	Will be used when testing Minos on the ARM FVP

# Run Minos on Raspberry Pi 3 Model B+

Minos has been tested on Raspberry Pi 3 Model b+, Raspberry Pi 3 and 3 Model A+ are supported too. These boards use Broadcom's bcm28737 chip, bcm2837 does not use GICv2 or GICv3 interrupt controllers which can support hardware interrupt virtualization. In order to implement interrupt virtualization on this chip and minimize Guest VM code modifications, the following method was adopted:

> * Implement virtual bcm2835-armctrl-ic and bcm2836-l1-intc interrupt controller for Host VM (VM0)
> * Extended the vGICv2 for the Guest VM

1. Install operating system images for 3B+

	Until now, there is no 64-bit operating system that fully supports 3B+ has been officially released on the Internet, including Debian or Ubuntu, but many netizens have provided their own versions.
Download the image and using dd to flash it to the SD card

        # wget http://leyunxi.com/static/debian-arm64-rpi3bplus.img
        # sudo dd bs=2M if=debian-arm64-rpi3bplus.img of=/dev/sdX

	After above commands when plug in the SD card to Ubuntu system again, the two partition will be mounted automaticlly, **the boot partition will be mount at /media/zac/6A99-E637, and the rootfs partition will be mounted at /media/zac/711a5ddf-1ff4-4d5a-ad95-8b9b69953513, the system used may not be the same. The following commands which modify the file on theses partitions, need to adjust the path according to the actual situation**.

2. Compile u-boot for 3B+

        # git clone https://github.com/u-boot/u-boot.git && cd u-boot
        # export CROSS_COMPILE=aarch64-linux-gnu-
        # make rpi_3_defconfig
        # make -j8
        # cp u-boot.bin /media/zac/6A99-E637/kernel8.img

3. Compile Linux Kernel

	The kernel using rpi-4.20.y branch from Raspberry offical git, and integrated the necessary Minos patches for bcm2837.

        # git clone https://github.com/minos-project/linux-raspberry.git && cd linux-raspberry
        # git checkout -b minos-rpi-4.20-y origin/minos-rpi-4.20-y
        # make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- bcmrpi3_defconfig
        # make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image -j8
        # make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules dtbs -j8
        # sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules_install INSTALL_MOD_PATH=/media/zac/711a5ddf-1ff4-4d5a-ad95-8b9b69953513
        # cp arch/arm64/boot/Image /media/zac/6A99-E637

	After above commands Raspberry kernel images and kernel moudules will be updated to 4.20

4. Compile Minos

        # git clone https://github.com/minos-project/minos-hypervisor.git && cd minos-hypervisor
        # make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu- rpi_3_defconfig
        # make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu-
        # make dtbs
        # cp hypervisor/minos.bin /media/minle/6A99-E637  (Minos hypervisor binary)
        # cp hypervisor/dtbs/bcm2837-rpi-3-b-plus.dtb /media/minle/6A99-E637
        # sudo cp mvm/mvm /media/zac/711a5ddf-1ff4-4d5a-ad95-8b9b69953513/home/jiangxianxu  (Minos tools for VM0)

5. Copy Guest VM boot.img to rootfs

        # cd minos-samples
        # sudo cp aarch32-boot.img /media/zac/711a5ddf-1ff4-4d5a-ad95-8b9b69953513/home/jiangxianxu/boot32.img
        # sudo cp aarch64-boot.img /media/zac/711a5ddf-1ff4-4d5a-ad95-8b9b69953513/home/jiangxianxu/boot64.img

	The minos-sample provides the dts/dtb file of the Guest VM and the created Guest VM boot.img file.

6. Modify Raspberry config.txt for Minos

        arm_control=0x200
        #dtoverlay=pi3-miniuart-bt
        enable_uart=1
        kernel=kernel8.img

6. Boot system with Minos support

	Boot the Raspberry to the u-boot commandline environment, execute the following command to start the system

        fatload mmc 0:1 0x28000000 minos.bin
        fatload mmc 0:1 0x03e00000 bcm2837-rpi-3-b-plus.dtb
        fatload mmc 0:1 0x00080000 Image
        booti 0x28000000 - 0x03e00000

	The Debian login username name is **jiangxianxu**, and the password is **linux**, if need to connect to the wifi network please modify ssid and psk in /etc/wpa_supplicant/wpa_supplicant.conf. After connected to the network can use below command to login into the system

        # ssh -p 1314 jiangxianxu@xxx.xxx.xxx.xxx

7. Create new guest VM

	Below commands are used to create 2 VMS, one is 64bit and another is 32bit.

        # cd /home/jiangxianxu
        # sudo chmod 777 mvm
        # sudo ./mvm -c 2 -m 96M -i boot64.img -n elinux64 -t linux -b 64 -v -r -d --gicv2 --earlyprintk -V virtio_console,@pty: -V virtio_blk,/home/jiangxianxu/sd.img -V virtio_net,tap0 -C "console=hvc0 loglevel=8 consolelog=9 root=/dev/vda2 rw"   (64bit VM with virtio_console; virtio_net and virtio_blk devices)
        # sudo ./mvm -c 2 -m 96M -i boot32.img -n elinux32 -t linux -b 32 -v -d --gicv2 --earlyprintk -V virtio_console,@pty: -C "console=hvc0 loglevel=8 consolelog=9"   (32bit VM with virtio console device)

# Run Minos on Marvell Esspressobin

1. Compile Minos

        # cd ~/minos-workspace/minos-hypervisor
        # make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu- espressobin_defconfig
        # make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu-
        # make dtbs   (new dtb file "armada-3720-community-v5.dtb" will be generate at hypervisor/dtbs folder)

	The default platform for Minos is Marvel Esspressobin. After the compilation is completed, minos.bin will be generated in the hypervisor/out directory and the mvm application will be generated in the mvm directory.

2. Compile Marvell Linux Kernel

        # cd ~/minos-workspace/linux-marvell
        # export ARCH=arm64
        # export CROSS_COMPILE=aarch64-linux-gnu-
        # make mvebu_v8_lsp_defconfig
        # make -j4

	After the compilation is complete, the kernel binary image will be generated in the arch/arm64/boot directory.

3. The default kernel of Esspressobin is stored in the /boot directory of the development board. Copy the minos.bin and the new Kernel Image to the /boot directory, and copy the mvm application to the user root directory of the development board.

4. Update Uboot boot settings of the development board

	Start the development board to the command line environment, execute the following command to update the Uboot startup settings (here is the example of the EMMC version of the Esspressobin).

        # setenv bootcmd “mmc dev 1; ext4load mmc 1:1 0x3c000000 boot/minos.bin; ext4load mmc 1:1 0x280000 boot/Image; ext4load mmc 1:1 0xfe00000 boot/armada-3720-community-v5.dtb; setenv bootargs console=ttyMV0,115200 earlycon=ar3700_uart,0xd0012000 root=PARTUUID=89708921-01 rw rootwait net.ifnames=0 biosdevname=0; booti 0x3c000000 - 0xfe00000”
        # saveenv

5. After the setup is complete, restart the development board, then every time the board startup, it will first jump to the Minos to execute virtualization related settings, and then start VM0.

	Tip: If the system cannot be started because of the Minos code error, just start the non-virtualized environment with the original startup parameters, and then replace the right minos.bin to the /boot directory.

        # mmc dev 1; ext4load mmc 1:1 $kernel_addr $image_name; ext4load mmc 1:1 $fdt_addr $fdt_name; setenv bootargs $console root=PARTUUID=89708921-01 rw rootwait net.ifnames=0 biosdevname=0; booti $kernel_addr - $fdt_addr

# Run Minos on ARM FVP

1. Download ARM FVP and create a working directory

        # mkdir ~/minos-workspace/arm-fvp

	FVP can be downloaded from ARM's official website. Minos supports FVP_Base_AEMv8A and FVP_Base_Cortex-A57x2-A53x4. Here we use FVP_Base_AEMv8A to do the testing. In addition, if you want to do related development based on Minos, you can also directly install the ARM DS5 debugging tool, and bring the above two FVPs after installation. The following is a tutorial on installing and using DS5.

- **ARM FVP (Fixed Virtual Platform) Linux Kernel Debugging Concise Manual:**[https://www.jianshu.com/p/c0a9a4b9569d](https://www.jianshu.com/p/c0a9a4b9569d)

2. Compile Minos

        # cd ~/minos-workspace/minos
        # make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu- fvp_defconfig
        # make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu-
        # make dtbs

3. Compile FVP Kernel

        # cd ~/minos-workspace/linux-marvell
        # make ARCH=arm64 defconfig && make ARCH=arm64 -j8 Image

4. Compile ARM Trusted Firmware

        # cd ~/minos-workspace/arm-trusted-firmware
        # make PLAT=fvp RESET_TO_BL31=1 ARM_LINUX_KERNEL_AS_BL33=1 PRELOADED_BL33_BASE=0xc0000000 ARM_PRELOADED_DTB_BASE=0x83e00000 ENABLE_PIE=0

5. Download ARM64 virtio-block image

        # cd ~/minos-workspace
        # wget https://releases.linaro.org/archive/14.07/openembedded/aarch64/vexpress64-openembedded_minimal-armv8-gcc-4.9_20140727-682.img.gz
        # gunzip vexpress64-openembedded_minimal-armv8-gcc-4.9_20140727-682.img.gz
        # mv vexpress64-openembedded_minimal-armv8-gcc-4.9_20140727-682.img sd.img

6. Run FVP with Minos

        # cd ~/minos-workspace/arm-fvp
        # ln -s ~/minos-workspace/sd.img sd.img
        # ln -s ~/minos-workspace/arm-trusted-firmware/build/fvp/release/bl31.bin bl31.bin
        # ln -s ~/minos-workspace/linux-marvell/arch/arm64/boot/Image Image
        # ln -s ~/minos-workspace/minos-hypervisor/hypervisor/dtbs/foundation-v8-gicv3.dtb fdt.dtb
        # ln -s ~/minos-workspace/minos-hypervisor/hypervisor/minos.bin minos.bin

        # /usr/local/DS-5_v5.27.0/bin/FVP_Base_AEMv8A               \
        -C pctl.startup=0.0.0.0                                     \
        -C bp.secure_memory=0                                       \
        -C cluster0.NUM_CORES=4                                     \
        -C cache_state_modelled=1                                   \
        -C cluster0.cpu0.RVBAR=0x04020000                           \
        -C cluster0.cpu1.RVBAR=0x04020000                           \
        -C cluster0.cpu2.RVBAR=0x04020000                           \
        -C cluster0.cpu3.RVBAR=0x04020000                           \
        -C bp.hostbridge.userNetPorts="8022=22"                     \
        -C bp.hostbridge.userNetworking=true                        \
        -C bp.dram_size=8                                           \
        -C bp.smsc_91c111.enabled=true                              \
        -C bp.virtioblockdevice.image_path=sd.img                   \
        --data cluster0.cpu0=bl31.bin@0x04020000                    \
        --data cluster0.cpu0=fdt.dtb@0x83e00000                     \
        --data cluster0.cpu0=Image@0x80080000                       \
        --data cluster0.cpu0=minos.bin@0xc0000000

8. After starting FVP, you can run the following command on the host to log in to FVP through ssh.

        # ssh -p 8022 root@127.0.0.1

# MVM usage

Minos provides two ways to create a VM. One is to use the dts file under the Minos source (for example, hypervisor/dtbs/foundation-v8-gicv3.dts) to create a corresponding VM by creating a device tree node. This method is suitable for creating VMs with real hardware permissions in embedded systems. Minos supports assigning specific hardware devices to specific VMs. VMs created this way are currently not managed by mvm.

```
vms {
		vm0 {
			device_type = "virtual_machine";
			vmid = <0>;
			vm_name = "fvp_linux_host";
			type = "linux";
			vcpus = <1>;
			entry = <0x0 0x80080000>;
			vcpu_affinity = <0 1>;
			memory = <0x0 0x80000000 0x0 0x8000000>;
		};
	};
```

Another way is to use the VM management tool mvm provided by Minos. Currently mvm already supports VM creation, destruction, restart and shutdown operations.

        Usage: mvm [options]

        -c <vcpu_count>            (set the vcpu numbers of the vm)
        -m <mem_size_in_MB>        (set the memsize of the vm - 2M align)
        -i <boot or kernel image>  (the kernel or bootimage to use)
        -s <mem_start>             (set the membase of the vm if not a boot.img)
        -n <vm name>               (the name of the vm)
        -t <vm type>               (the os type of the vm )
        -b <32 or 64>              (32bit or 64 bit )
        -r                         (do not load ramdisk image)
        -v                         (verbose print debug information)
        -d                         (run as a daemon process)
        -D                         (create a platform bus device)
        -V                         (create a virtio device)
        -C                         (set the cmdline for the os)
        -K                         (kernel image path)
        -S                         (second image path - like dtb image)
        -R                         (Ramdisk image path)
        --gicv2                    (using the gicv2 interrupt controller)
        --gicv3                    (using the gicv3 interrupt controller - default value)
        --gicv4                    (using the gicv4 interrupt controller - not support now)
        --earlyprintk              (enable the earlyprintk based on virtio-console)

For example, the following command is used to create a Linux virtual machine with 2 vcpu, 84M memory, bootimage as boot.img, and 64-bit with virtio-console device and virtio-net device. Below command will use ramdisk in boot.img as the rootfs instead of block device.

        # ./mvm -c 2 -m 84M -i boot.img -n elinux -t linux -b 64 -v -d -C "console=hvc0 loglevel=8 consolelog=9" -V virtio_console,@pty: -V virtio_net,tap0

Now Minos also support using virtio block device as the root device, below command will create a linux vm using virtio-blk device as root device instead of ramdisk

        # ./mvm -c 1 -m 64M -i boot.img -n linux -t linux -b 64 -v -r -d -V virtio_console,@pty: -V virtio_blk,~/minos-workspace/sd.img -V virtio_net,tap0 -C "console=hvc0 loglevel=8 consolelog=9 root=/dev/vda2 rw"

If the creation is successful, the following log output will be generated.

        [INFO ] no rootfs is point using ramdisk if exist
        root@genericarmv8:~# [INFO ] boot image infomation :
        [INFO ] magic        - ANDROID!
        [INFO ] kernel_size  - 0x877800
        [INFO ] kernel_addr  - 0x80080000
        [INFO ] ramdisk_size - 0x104e21
        [INFO ] ramdisk_addr - 0x83000000
        [INFO ] dtb_size     - 0xcc4
        [INFO ] dtb_addr     - 0x83e00000
        [INFO ] tags_addr    - 0x0
        [INFO ] page_size    - 0x800
        [INFO ] name         -
        [INFO ] cmdline      - console=hvc0 loglevel=8 consolelog=9
        [INFO ] create new vm *
        [INFO ]         -name       : elinux
        [INFO ]         -os_type    : linux
        [INFO ]         -nr_vcpus   : 2
        [INFO ]         -bit64      : 1
        [INFO ]         -mem_size   : 0x5400000
        [INFO ]         -mem_start  : 0x80000000
        [INFO ]         -entry      : 0x80080000
        [INFO ]         -setup_data : 0x83e00000
        [DEBUG] load kernel image: 0x80000 0x800 0x877800
        [DEBUG] load ramdisk image:0x3000000 0x878000 0x104e21
        [DEBUG] vdev : irq-32 gpa-0x7fad895000 gva-0x40000000
        [INFO ] ***********************************************
        [INFO ] virt-console backend redirected to /dev/pts/1
        [INFO ] ***********************************************
        [INFO ] add cmdline - console=hvc0 loglevel=8 consolelog=9 loglevel=8 consolelog=9
        [INFO ]         - delete cpu@2
        [INFO ]         - delete cpu@3
        [INFO ]         - delete cpu@4
        [INFO ]         - delete cpu@5
        [INFO ]         - delete cpu@6
        [INFO ]         - delete cpu@7
        [DEBUG] found 1 rsv memory region
        [DEBUG] add rsv memory region : 0x80000000 0x10000
        [INFO ] setup memory 0x0 0x80 0x0 0x4005
        [INFO ] set ramdisk : 0x83000000 0x104e21
        [INFO ] add vdev success addr-0x40000000 virq-32

Minos currently supports the virtio-console backend driver. After creating the VM, you can log in to the VM with terminal tools such as minicom. (In FVP, you need to wait for a while. The VM startup speed depends on the performance of the host. You can turn off the FVP's cache to speed up the startup.)

        # minicom /dev/pts/1

# Create a customize bootimage

The default ramdisk.img in the boot.img provided by Minos is based on the default rootfs configuration of the busybox. If you need to customize your own ramdisk, it is also very simple. You only need to repackage the ramdisk.img, Image and dtb file. There are two samples boot image under minos-samples folder.

        # dtc -I dts -O dtb -o guest-vm.dtb guest-vm.dts
        # abootimg --create boot.img -c kerneladdr=0x80080000 -c ramdiskaddr=0x83000000 -c secondaddr=0x83e00000 -c cmdline="console=hvc0 loglevel=8 consolelog=9" -k Image -s guest-vm.dtb -r ramdisk.img

# Create a customize virtio-blk image

Minos provide a sample virtio-blk image which size is only 512M, the sample virtio block image can be download, various virtio block images can be also downloaded from Linaro:

        # wget http://leyunxi.com/static/sd.img

The below commands is to create a bigger block image with 2G size and LAMP support. All the tools and commands are from Linaro, and test on Ubuntu 14.04, more information can refers to [Linaro release https://releases.linaro.org/openembedded/aarch64/15.08/](https://releases.linaro.org/openembedded/aarch64/15.08/)

        # sudo add-apt-repository ppa:linaro-maintainers/tools
        # sudo apt-get update
        # sudo apt-get install linaro-image-tools
        # wget https://releases.linaro.org/openembedded/aarch64/15.08/linaro-image-lamp-genericarmv8-20150729-758.rootfs.tar.gz
        # wget https://releases.linaro.org/openembedded/aarch64/15.08/hwpack_linaro-vexpress64-rtsm_20150821-726_arm64_supported.tar.gz
        # linaro-media-create --dev fastmodel --output-directory fastmodel --image_size 2048M --hwpack hwpack_linaro-vexpress64-rtsm_20150821-726_arm64_supported.tar.gz --binary linaro-image-lamp-genericarmv8-20150729-758.rootfs.tar.gz
