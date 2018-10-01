# Minos - Type 1 Hypervisor for ARMv8-A

Minos是一款轻量级的面向移动及嵌入式系统的开源Type 1 Hypervisor, 直接运行于裸机环境。Minos实现了一套完整的虚拟化框架，可以在同一硬件平台上同时运行多个不同操作系统的VM(Linux or RTOS). Minos提供了包括CPU虚拟化; 中断虚拟化; 内存虚拟化; Timer虚拟化; 以及一些常用外设虚拟化的支持。

Minos提供一个运行于VM0上的应用程序"mvm"来支持Guest VM的管理。同时mvm提供基于virtio的半虚拟化解决方案, 支持virtio-console, virtio-blk(测试中)，virtio-net(测试中)等设备。

Minos适用于移动及嵌入式平台，目前只支持ARMv8-A架构。硬件上支持Marvell的Esspressobin开发板，且理论上ARMv8-A + GICV3组合的硬件平台都可以被支持。软件调试平台支持ARM官方的Fix Virtual Platform (简称FVP), 开发者可以用ARM DS5工具来进行仿真和调试。

# Download Source Code And Tools for Minos

1. 创建工作目录

        # mkdir ~/minos-workspace
        # cd ~/minos-workspace

2. 安装gcc交叉编译工具

        # wget https://releases.linaro.org/components/toolchain/binaries/latest/aarch64-linux-gnu/gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu.tar.xz
        # tar xjf gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu.tar.xz
        # sudo mv gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu /opt
        # echo "export PATH=/opt/gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu/bin:$PATH" >> ~/.bashrc
        # source ~/.bashrc

3. 安装abootimg工具

        # sudo apt-get install abootimg

	abootimg 工具用来制作Linux VM的bootimge，mvm使用此格式image来加载linux内核，ramdisk和dtb文件

4. 安装device tree代码编译工具

        # sudo apt-get install device-tree-compiler

5. 下载Minos sample

        # git clone https://github.com/minos-project/minos-samples.git

	minos-sample提供了Guest VM的dts/dtb文件，以及制作好的Guest VM boot.img文件

6. 下载Minos hypervisor 源码

        # git clone https://github.com/minos-project/minos-hypervisor.git

7. 下载Linux Kernel 源码

        # git clone https://github.com/minos-project/linux-marvell.git
        # cd linux-marvell
        # git checkout -b minos origin/minos

	默认下载的是添加了Minos驱动的Marvell平台的Linux Kernel, 如果用的是别的硬件平台，只需要添加Minos驱动就行，下面命令可以获取Minos驱动以及必要的Kernel Patch

        # git clone  https://github.com/minos-project/minos-linux-driver.git

8. 下载ATF源码

        # git clone https://github.com/ARM-software/arm-trusted-firmware.git

	在FVP上运行和调试Minos时需要用到

# Run Minos on Marvell Esspressobin

1. 编译Minos

        # cd ~/minos-workspace/minos
        # make

	Minos默认平台为Marvel Esspressobin，编译完成后会在 hypervisor/out目录下生成minos.bin以及在mvm目录下生成mvm应用程序

2. 编译Marvell Linux Kernel

        # cd ~/minos-workspace/linux-marvell
        # export ARCH=arm64
        # export CROSS_COMPILE=aarch64-linux-gnu-
        # make mvebu_v8_lsp_defconfig
        # make -j4

	编译完成后会在arch/arm64/boot目录下生成Image内核二进制文件。

3. Esspressobin默认的内核存放在开发板的/boot目录下，把minos.bin和新的Kernel Image拷贝到/boot目录下, 并把mvm应用拷贝到开发板的用户根目录下。

4. 更新开发板Uboot启动设置

	启动开发板到命令行状态，执行以下命令更新Uboot启动设置（这里以EMMC版本的Esspressobin开发板举例，采用SD卡方式启动的开发板，方法类似)

        # setenv bootcmd “mmc dev 1; ext4load mmc 1:1 0x3c000000 boot/minos.bin; ext4load mmc 1:1 0x280000 boot/Image; ext4load mmc 1:1 0xfe00000 boot/armada-3720-community-v5.dtb; setenv bootargs console=ttyMV0,115200 earlycon=ar3700_uart,0xd0012000 root=PARTUUID=89708921-01 rw rootwait net.ifnames=0 biosdevname=0; booti 0x3c000000 - 0xfe00000”
        # saveenv

5. 设置完之后重启开发板，之后每次开机将会先跳转到Minos执行hypervisor相关设置，然后再启动VM0

	提示: 如果因为Minos代码错误导致系统启动不了，只需要用原来的启动参数先启动到非虚拟化环境，然后把能正常运行的minos.bin替换到/boot目录下就可以

        # mmc dev 1; ext4load mmc 1:1 $kernel_addr $image_name; ext4load mmc 1:1 $fdt_addr $fdt_name; setenv bootargs $console root=PARTUUID=89708921-01 rw rootwait net.ifnames=0 biosdevname=0; booti $kernel_addr - $fdt_addr

![Run Minos on Marvell Board](http://leyunxi.com/static/minos-marvell-00.png)

# Run Minos on ARM FVP

1. 下载ARM FVP,创建工作目录

        # mkdir ~/minos-workspace/arm-fvp

	FVP可以在ARM的官网下载，Minos支持FVP_Base_AEMv8A 以及FVP_Base_Cortex-A57x2-A53x4 ，这里我们默认使用FVP_Base_AEMv8A来进行测试。另外如果想基于Minos做相关开发，也可以直接安装ARM DS5调试工具，安装完之后自带以上两个FVP。以下是安装使用DS5的相关教程

- **ARM FVP(固定虚拟平台)Linux内核调试简明手册:**[https://www.jianshu.com/p/c0a9a4b9569d](https://www.jianshu.com/p/c0a9a4b9569d)

2. 编译Minos

        # cd ~/minos-workspace/minos
        # make distclean  (每次改变编译target前需要执行 make distclean)
        # make PLATFORM=fvp

3. 编译FVP Kernel

        # cd ~/minos-workspace/minos
        # make ARCH=arm64 defconfig && make ARCH=arm64 -j8 Image

4. 编译ARM Trusted Firmware

        # cd ~/minos-workspace/arm-trusted-firmware
        # make PLAT=fvp RESET_TO_BL31=1 ARM_LINUX_KERNEL_AS_BL33=1 PRELOADED_BL33_BASE=0xc0000000 ARM_PRELOADED_DTB_BASE=0x83e00000

5. 下载ARM64 virtio-block image

        # cd ~/minos-workspace
        # wget https://releases.linaro.org/archive/14.07/openembedded/aarch64/vexpress64-openembedded_minimal-armv8-gcc-4.9_20140727-682.img.gz
        # gunzip vexpress64-openembedded_minimal-armv8-gcc-4.9_20140727-682.img.gz
        # mv vexpress64-openembedded_minimal-armv8-gcc-4.9_20140727-682.img sd.img

6. 运行FVP

        # cd ~/minos-workspace/arm-fvp
        # ln -s ~/minos-workspace/sd.img sd.img
        # ln -s ~/minos-workspace/arm-trusted-firmware/build/fvp/release/bl31.bin bl31.bin
        # ln -s ~/minos-workspace/linux-marvell/arch/arm64/boot/Image Image
        # ln -s ~/minos-workspace/minos-sample/foundation-v8-gicv3.dtb fdt.dtb
        # ln -s ~/minos-workspace/minos/hypervisor/out/minos.bin minos.bin

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

8. 启动FVP之后，可以在主机上运行以下命令通过ssh来登入FVP

        # ssh -p 8022 root@127.0.0.1

![Run Minos on FVP ](http://leyunxi.com/static/minos-fvp-00.png)

# mvm使用方法

Minos提供两种方式来创建VM, 一种是使用Minos源码下的JSON文件(例如hypervisor/config/fvp/fvp.json.cc)，通过创建一个vmtag的json成员来创建对应的VM，且这种VM的内存, IRQ等硬件资源都是通过对应的json文件来管理的，此方式适合用来创建嵌入式系统中拥有真实硬件权限的VM, Minos支持将特定的硬件设备分配给特定的VM。通过这种方式创建的VM当前没法被mvm管理。

```
#include "fvp_config.h"
{
	"version": "0.0.1",
	"platform": "armv8-fvp",

	"vmtags": [
	{
			"vmid": 0,
			"name": "linux-01",
			"type": "linux",
			"nr_vcpu": 1,
			"entry": "0x80080000",
			"setup_data": "0x83e00000",
			"vcpu0_affinity": 0,
			"vcpu1_affinity": 1,
			"vcpu2_affinity": 2,
			"vcpu3_affinity": 3,
			"cmdline": "",
			"bit64": 1
		}
	],
	#include "fvp_irq.json.cc"
	#include "fvp_mem.json.cc"

	"others" : {
		"comments": "minos virtualization config json data"
	}
}
```

另外一种方式就是通过Minos提供的VM管理工具mvm来配置, 当前mvm已经支持了VM的创建，销毁，重启和关机操作。

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
        -D                         (device argument)
        -C                         (set the cmdline for the os)

例如以下命令用来创建一个拥有 2 个vcpu， 84M内存， bootimage为boot.img以及带有virtio-console设备的64位的(当前Minos只支持64位VM)Linux虚拟机.

        #./mvm -c 2 -m 84M -i boot.img -n elinux -t linux -b 64 -v -d -C "console=hvc0 loglevel=8 consolelog=9 loglevel=8 consolelog=9" -D virtio_console,@pty:

创建成功的话会有以下log输出

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

Minos当前已经支持virtio-console后端驱动，创建完VM之后可以用minicom等终端工具登入VM

        # minicom /dev/pts/1

![minicom to connect VM](http://leyunxi.com/static/minos-fvp-01.png)

# 制作自定义bootimage

Minos默认提供的boot.img的ramdisk.img基于busybox默认rootfs配置,如果需要自定义自己定制ramdisk,也很简单，只需要将制作好ramdisk.img和Image以及dtb文件重新打包:

        # dtc -I dts -O dtb -o guest-vm.dtb guest-vm.dts
        # abootimg --create boot.img -c kerneladdr=0x80080000 -c ramdiskaddr=0x83000000 -c secondaddr=0x83e00000 -c cmdline="console=hvc0 loglevel=8 consolelog=9" -k Image -s guest-vm.dtb -r ramdisk.img

