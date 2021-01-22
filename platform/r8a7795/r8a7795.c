// SPDX-License-Identifier: GPL-2.0

#include <asm/power.h>
#include <minos/platform.h>

static struct platform platform_r8a7795 = {
    .name            = "renesas,r8a7795",
    .cpu_on          = psci_cpu_on,
    .cpu_off         = psci_cpu_off,
    .system_reboot   = psci_system_reboot,
    .system_shutdown = psci_system_shutdown,
};

DEFINE_PLATFORM(platform_r8a7795);
