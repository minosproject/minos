/*
 * Copyright (C) 2019 Min Le (lemin9538@gmail.com)
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

#include <minos/minos.h>
#include <asm/svccc.h>
#include <minos/sched.h>
#include <asm/psci.h>
#include <virt/vm.h>

#define SECMON_IN_BASE_FUNC		0x82000020
#define SECMON_OUT_BASE_FUNC		0x82000021

#define SECKEY_STORAGE_QUERY		0x82000060
#define SECKEY_STORAGE_READ		0x82000061
#define SECKEY_STORAGE_WRITE		0x82000062
#define SECKEY_STORAGE_TELL		0x82000063
#define SECKEY_STORAGE_VERIFY		0x82000064
#define SECKEY_STORAGE_STATUS		0x82000065
#define SECKEY_STORAGE_LIST		0x82000067
#define SECKEY_STORAGE_REMOVE		0x82000068
#define SECKEY_STORAGE_IN_FUNC		0x82000023
#define SECKEY_STORAGE_OUT_FUNC		0x82000024
#define SECKEY_STORAGE_BLOCK_FUNC	0x82000025
#define SECKEY_STORAGE_SIZE_FUNC	0x82000027
#define SECKEY_STORAGE_SET_ENCTYPE	0x8200006a
#define SECKEY_STORAGE_GET_ENCTYPE	0x8200006b
#define SECKEY_STORAGE_VERSION		0x8200006c

#define EFUSE_READ_CMD			0x82000030
#define EFUSE_WRITE_CMD			0x82000031
#define EFUSE_GET_MAX_CMD		0x82000033

#define CPUINFO_CMD			0x82000044

#define AUDIO_QUERY_LICENSE_CMD		0x82000050

#define AMLOGIC_SMC_BASE		0x82000000
#define AMLOGIC_SMC_HANDLER_NUMBER	128
#define AMLOGIC_SMC_END			(AMLOGIC_SMC_BASE + AMLOGIC_SMC_HANDLER_NUMBER)

static svc_handler_t amlogic_smc_fn[128];

static int amlogic_unknown_smc(gp_regs *c, uint32_t id, unsigned long *args)
{
	struct arm_smc_res res;

	smc_call(id, args[0], args[1], args[2], args[3], 0, 0, 0, &res);
	SVC_RET4(c, res.a0, res.a1, res.a2, res.a3);

	return 0;
}

static int sip_smc_handler(gp_regs *c, uint32_t id, unsigned long *args)
{
	struct vcpu *vcpu = get_current_vcpu();
	struct vm *vm = vcpu->vm;
	svc_handler_t fn;

	pr_debug("sip function for amlogic 0x%x\n", id);

	if (!vm_is_hvm(vm))
		return -EPERM;

	fn = amlogic_smc_fn[id - AMLOGIC_SMC_BASE];
	if (fn)
		return fn(c, id, args);
	else
		return amlogic_unknown_smc(c, id, args);
}
DEFINE_SMC_HANDLER("sip_smc_desc", SVC_STYPE_SIP,
		SVC_STYPE_SIP, sip_smc_handler);

static int inline amlogic_install_smc(svc_handler_t fn, uint32_t id)
{
	if ((id < AMLOGIC_SMC_BASE) || (id > AMLOGIC_SMC_END))
		return -EINVAL;

	amlogic_smc_fn[id - AMLOGIC_SMC_BASE] = fn;
	return 0;
}

static inline int amlogic_smc_call(gp_regs *c, uint32_t id)
{
	struct arm_smc_res res;

	smc_call(id, 0, 0, 0, 0, 0, 0, 0, &res);
	SVC_RET1(c, res.a0);

	return 0;
}

static inline int
amlogic_smc_call2(gp_regs *c, uint32_t id, unsigned long arg)
{
	struct arm_smc_res res;

	smc_call(id, arg, 0, 0, 0, 0, 0, 0, &res);
	SVC_RET1(c, res.a0);

	return 0;
}

static int secmon_in_base(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int secmon_out_base(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int seckey_storage_query(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int seckey_storage_read(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int seckey_storage_write(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int seckey_storage_tell(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int seckey_storage_verify(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int seckey_storage_status(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int seckey_storage_list(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int seckey_storage_remove(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int seckey_storage_in_func(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int seckey_storage_out_func(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int seckey_storage_block_func(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int seckey_storage_size_func(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int seckey_storage_get_enctype(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int seckey_storage_set_enctype(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call2(c, id, args[0]);
}

static int seckey_storage_version(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int efuse_read_cmd(gp_regs *c, uint32_t id, unsigned long *args)
{
	struct arm_smc_res res;

	smc_call(id, args[0], args[1], 0, 0, 0, 0, 0, &res);
	SVC_RET1(c, res.a0);

	return 0;
}

static int efuse_write_cmd(gp_regs *c, uint32_t id, unsigned long *args)
{
	struct arm_smc_res res;

	smc_call(id, args[0], args[1], 0, 0, 0, 0, 0, &res);
	SVC_RET1(c, res.a0);

	return 0;
}

static int efuse_get_max_cmd(gp_regs *c, uint32_t id, unsigned long *args)
{
	return amlogic_smc_call(c, id);
}

static int cpuinfo_cmd(gp_regs *c, uint32_t id, unsigned long *args)
{
	struct arm_smc_res res;

	smc_call(id, args[0], args[1], args[2], 0, 0, 0, 0, &res);
	SVC_RET1(c, res.a0);

	return 0;
}

static int audio_query_license_cmd(gp_regs *c, uint32_t id, unsigned long *args)
{
	struct arm_smc_res res;

	smc_call(id, args[0], args[1], 0, 0, 0, 0, 0, &res);
	SVC_RET1(c, res.a0);

	return 0;
}

static int __init_text amlogic_smc_init(void)
{
	amlogic_install_smc(secmon_in_base, SECMON_IN_BASE_FUNC);
	amlogic_install_smc(secmon_out_base, SECMON_OUT_BASE_FUNC);

	amlogic_install_smc(seckey_storage_query, SECKEY_STORAGE_QUERY);
	amlogic_install_smc(seckey_storage_read, SECKEY_STORAGE_READ);
	amlogic_install_smc(seckey_storage_write, SECKEY_STORAGE_WRITE);
	amlogic_install_smc(seckey_storage_tell, SECKEY_STORAGE_TELL);
	amlogic_install_smc(seckey_storage_verify, SECKEY_STORAGE_VERIFY);
	amlogic_install_smc(seckey_storage_status, SECKEY_STORAGE_STATUS);
	amlogic_install_smc(seckey_storage_list, SECKEY_STORAGE_LIST);
	amlogic_install_smc(seckey_storage_remove, SECKEY_STORAGE_REMOVE);
	amlogic_install_smc(seckey_storage_in_func, SECKEY_STORAGE_IN_FUNC);
	amlogic_install_smc(seckey_storage_out_func, SECKEY_STORAGE_OUT_FUNC);
	amlogic_install_smc(seckey_storage_block_func, SECKEY_STORAGE_BLOCK_FUNC);
	amlogic_install_smc(seckey_storage_size_func, SECKEY_STORAGE_SIZE_FUNC);
	amlogic_install_smc(seckey_storage_get_enctype, SECKEY_STORAGE_SET_ENCTYPE);
	amlogic_install_smc(seckey_storage_set_enctype, SECKEY_STORAGE_GET_ENCTYPE);
	amlogic_install_smc(seckey_storage_version, SECKEY_STORAGE_VERSION);

	amlogic_install_smc(efuse_read_cmd, EFUSE_READ_CMD);
	amlogic_install_smc(efuse_write_cmd, EFUSE_WRITE_CMD);
	amlogic_install_smc(efuse_get_max_cmd, EFUSE_GET_MAX_CMD);

	amlogic_install_smc(cpuinfo_cmd, CPUINFO_CMD);
	amlogic_install_smc(audio_query_license_cmd, AUDIO_QUERY_LICENSE_CMD);

	return 0;
}
module_initcall(amlogic_smc_init);
