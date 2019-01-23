/*
 * Copyright (C) 2018 - 2019 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <minos/types.h>

char *gicv2_match_table[] = {
	"arm,gicv2",
	"arm,gic-400",
	"arm,arm11mp-gic",
	"arm,arm1176jzf-devchip-gic",
	"arm,cortex-a15-gic",
	"arm,cortex-a9-gic",
	"arm,cortex-a7-gic",
	"qcom,msm-8660-qgic",
	"qcom,msm-qgic2",
	NULL
};

char *gicv3_match_table[] = {
	"arm,gic-v3",
	NULL
};

char *bcmirq_match_table[] = {
	"brcm,bcm2836-l1-intc",
	NULL
};

char *pl031_match_table[] = {
	"arm,pl031",
	NULL
};

char *sp805_match_table[] = {
	"arm,sp805",
	NULL
};

char *virtio_match_table[] = {
	"virtio,mmio",
	NULL
};
