# Minos - Type 1 Hypervisor for ARMv8-A

Minos is a lightweight open source Type 1 Hypervisor for mobile and embedded systems that runs directly in bare metal environments. Minos implements a complete virtualization framework that can run multiple VMs (Linux or RTOS) on one hardware platform. Minos provides CPU virtualization; interrupt virtualization; memory virtualization; Timer virtual; and the virtualization of some common peripherals.

Minos provides an application "mvm" running on VM0 to support the management of the Guest VM. At the same time, mvm provides a virtio-based paravirtualization solution that supports virtio-console, virtio-blk, virtio-net and other devices. Minos can support both 64bit and 32bit guest VM, but VM0 must use 64bit.

Minos is suitable for mobile and embedded platforms and currently only supports the ARMv8-A architecture. Marvell's Esspressobin and Raspberry Pi 3 are supported, and the hardware platform of the ARMv8-A + GICV3/GICV2 combination can theoretically be supported. The software debugging platform supports ARM's official Fix Virtual Platform (FVP), and developers can use ARM DS5 tools for simulation and debugging.

Below is the board that Minos has been supported:

- [x] Marvell's Esspressobin development board
- [x] Raspberry Pi 3 Model B+ / Raspberry Pi 3 Model A+ / Raspberry Pi 3 Model B
- [x] ARMv8 Fixed Virtual Platforms

Here is a video shows how to run Minos on Raspberry Pi 3B+ [Run Minos Hypervisor On Raspberry 3B+](https://www.youtube.com/watch?v=82YrcGrkblo)
