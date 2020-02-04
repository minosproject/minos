/*
 * BSD 3-Clause License
 *
 * Copyright (C) 2020 Min Le (lemin9538@gmail.com)
 * All rights reserved.
 */

#include <minos/vm.h>
#include <minos/option.h>
#include <common/hypervisor.h>
#include <minos/vdev.h>

static int setup_vm_vdev(char *arg, char *sub_arg, void *data)
{
	return create_vdev((struct vm *)data, arg, sub_arg);
}
DEFINE_OPTION_VDEV(device, "device", 0, setup_vm_vdev);
