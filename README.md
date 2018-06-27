# MINOS
A small os with aarch64 virtualization support

基于armv8架构虚拟化技术实现的一个Hypervisor, 代码代码在arm的FVP上进行调试。具体进展如下:

- **CPU虚拟化:** 支持创建多个VM，且每个VM最多能创建和物理cpu同等数量的VCPU.当前只支持64位的VM
- **内存虚拟化:** 实现aarch64 VMSA,当前只实现管理最大4G物理内存空间，后续会支持full feature
- **中断虚拟化:** 实现了GICv3中断虚拟化支持
- **Timer虚拟化:** 实现了Generic Timer虚拟化支持
- **其它:** VCPU调度系统现在只支持最基础的调度逻辑。后期需要重点优化。

MINOS目标是实现成为一个支持虚拟化功能的实时操作系统，实现一个基于armv8架构的轻量级type 1 Hypervisor, 可应用于嵌入式相关产品。下面内容介绍如何运行和调试MINOS. 运行和调试过程中需要用到ARM的DS5集成开发调试环境，关于DS5的安装使用方法，在这里不再描述，可以参考以下文章（在Ubuntu 14.04验证成功，其它发型版本未测试）：

- **ARM FVP(固定虚拟平台)Linux内核调试简明手册:**[https://www.jianshu.com/p/c0a9a4b9569d](https://www.jianshu.com/p/c0a9a4b9569d)

下载获取运行调试MINOS所需要的工具和源码
======================================
需要以下工具和源码

- **Linux Kernel代码:** 用于测试minos的VM
- **FVP所使用的dts文件及编译工具:** ARM FVP device tree文件
- **ROOTFS:** 根文件系统
- **其它测试用代码:** VM测试代码
- **AARCH64 gcc toolchain:** arm64工具链

1. 创建工作目录

        mkdir ~/minos-workspace
        cd ~minos-workspace

2. 下载gcc toolchain [https://releases.linaro.org/components/toolchain/binaries/latest/aarch64-linux-gnu/](https://releases.linaro.org/components/toolchain/binaries/latest/aarch64-linux-gnu/) 当前最新版本7.3.1

        wget https://releases.linaro.org/components/toolchain/binaries/latest/aarch64-linux-gnu/gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu.tar.xz
        tar xjf gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu.tar.xz
        sudo mv gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu /opt
        echo "export PATH=/opt/gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu/bin:$PATH" >> ~/.bashrc
        source ~/.bashrc

3. 下载device tree代码编译工具

        sudo apt-get install device-tree-compiler

4. 下载minos源码

        git clone https://github.com/lemin9538/minos.git

4. 下载Linux Kernel源码

        git clone https://git.linaro.org/kernel/linux-linaro-stable.git -b linux-linaro-lsk-v4.1

5. 下载其它用测试代码及根文件系统

        git clone https://github.com/lemin9538/minos-test.git


代码编译
================

1. 编译minos

        cd minos
        make

2. 编译Linux kernel

        cd linux-linaro-stable
        export CROSS_COMPILE=/opt/gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
        make ARCH=arm64 defconfig
        make ARCH=arm64 defconfig

3. 编译device tree及Test VM

        cd minos-test
        make
        gunzip vexpress64-openembedded_minimal-armv8-gcc-4.9_20140727-682.img.gz
        mv vexpress64-openembedded_minimal-armv8-gcc-4.9_20140727-682.img sd.img


DS5设置
================================

在DS5里面新建一个调试Target(具体新建步骤请参考[https://www.jianshu.com/p/c0a9a4b9569d](https://www.jianshu.com/p/c0a9a4b9569d)), 然后把各设置项的内容设置为下图片内容(把图片中配置项中的路径改成~/minos-workspace)

![DS5 setting 01](http://leyunxi.com/static/s01.png)

![DS5 setting 02](http://leyunxi.com/static/s02.png)

![DS5 setting 03](http://leyunxi.com/static/s03.png)


以下为上图中Model Parameters及Excuted debuger commands具体设置内容
1. Model Parameters

        -C bp.secure_memory=false -C cache_state_modelled=0 -C bp.virtioblockdevice.image_path=~/minos-workspace/minos-test/sd.img


2. Excuted debuger commands

        restore "~/minos-workspace/linux-linaro-stable/arch/arm64/boot/Image"  binary EL1N:0x80080000
        restore "~/minos-workspace/minos-test/foundation-v8.dtb" binary EL1N:0x83e00000
        loadfile "~/minos-workspace/minos-test/os1.axf" EL1N:0x0
        add-symbol-file "~/minos-workspace/minos/out/minos.elf" EL2:0x0

以上设置设置好之后，就可以运行了

![DS5 running](http://leyunxi.com/static/4vcpu_ok.png)
