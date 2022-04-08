/*
 * BSD 3-Clause License
 *
 * Copyright (C) 2020 Min Le (lemin9538@gmail.com)
 * All rights reserved.
 */

#include <minos/vm.h>
#include <minos/option.h>
#include <generic/hypervisor.h>
#include <minos/os.h>

/*
 * do nothing to fix compile issue for option_os section
 */
DEFINE_OPTION_OS(setup_os_null, "os-res", 0, NULL);
