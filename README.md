# Minos 

http://minosproject.org

Microkernel RTOS with Virtualization and SMP support for ARMv8-A

## What is Minos

Minos is a real-time priority-based microkernel RTOS with virtualization support for ARMv8-A that provides the trusted reliability and performance for embedded system while also allowing multiple operating systems to safely co-exist on the same System on Chip (SoC). 

Minos defines a device hypervisor reference stack and an architecture for running multiple software subsystems, managed securely, on a consolidated system by means of a virtual machine manager. Minos can be used as a Type 1 reference hypervisor stack, running directly on the bare-metal hardware, and is suitable for a variety of IoT and embedded device solutions. Minos addresses the gap that currently exists between datacenter hypervisors, and hard partitioning hypervisors. The hypervisor architecture partitions the system into different functional domains, with carefully selected guest OS sharing optimizations for IoT and embedded devices.

Minos is also designed as a real-time priority-based microkernel RTOS that support SMP, currently support ARMv8-A, But can be easily ported to other platforms and architectures like Cortex-M MCU.

#### Supported hardware

- [x] Marvell espressobin

- [x] Raspberry 3B

- [x] Raspberry 4

- [x] Khadas VIM3

  Minos can be easily ported to other armv8-a based platform.

#### Supported OSes

- [x] Linux 
- [x] Ubuntu
- [x] Android
- [x] zephyr
- [x] XNU (ios kernel)

## Documentation

We will have various `README` files in the  `Documents` subdirectory. Please refer `Documents/000-INDEX` for a list of what is contained in each file or sub-directory.

1. [Documents/001-Build_Minos.md](https://github.com/minosproject/minos/blob/master/Documents/001-Build_Minos.md)
2. [Documents/002-Test_Minos_on_Raspberry_4.md](https://github.com/minosproject/minos/blob/master/Documents/002-Test_Minos_on_Raspberry_4.md)
3. [Documents/003-Test_Minos_on_Khadas_VIM3.md](https://github.com/minosproject/minos/blob/master/Documents/003-Test_Minos_on_Khadas_VIM3.md)
4. [Documents/004-Test_Minos_on_ARM_FVP.md](https://github.com/minosproject/minos/blob/master/Documents/004-Test_Minos_on_ARM_FVP.md)
5. [Documents/005-Test_IOS_kernel_using_Minos_on_VIM3.md](https://github.com/minosproject/minos/blob/master/Documents/005-Test_IOS_kernel_using_Minos_on_VIM3.md)
6. [Documents/006-How_to_use_mvm_create_guest_VM.md](https://github.com/minosproject/minos/blob/master/Documents/006-How_to_use_mvm_create_guest_VM.md)
7. [Documents/007-How_to_create_Native_VM.md](https://github.com/minosproject/minos/blob/master/Documents/007-How_to_create_Native_VM.md)
8. [Documents/008-Developer_Guide.md](https://github.com/minosproject/minos/blob/master/Documents/008-Developer_Guide.md)
9. [Documents/009-Test_Minos_on_Raspberry_3B.md](https://github.com/minosproject/minos/blob/master/Documents/009-Test_Minos_on_Raspberry_3B.md)