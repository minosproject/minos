#include <mvisor/mvisor.h>
#include <mvisor/os.h>
#include <mvisor/sched.h>

LIST_HEAD(os_list);
struct os *default_os;

static void default_vcpu_init(struct vcpu *vcpu)
{
	vcpu_online(vcpu);
}

struct os_ops default_os_ops = {
	.vcpu_init = default_vcpu_init,
};

int register_os(struct os *os)
{
	if ((!os) || (!os->ops))
		return -EINVAL;

	/* do not need lock now */
	list_add_tail(&os_list, &os->list);

	return 0;
}

struct os *alloc_os(char *name)
{
	struct os *os;

	os = (struct os *)zalloc(sizeof(struct os));
	if (!os)
		return NULL;

	init_list(&os->list);
	strncpy(os->name, name, MIN(strlen(name),
			MVISOR_OS_NAME_SIZE - 1));

	return os;
}

struct os *get_vm_os(char *type)
{
	struct os *os;

	if (!type)
		goto out;

	list_for_each_entry(os, &os_list, list) {
		if (!strcmp(os->name, type))
			return os;
	}

out:
	return default_os;
}

static int mvisor_os_init(void)
{
	default_os = alloc_os("default-os");
	if (!default_os)
		return -ENOMEM;

	default_os->ops = &default_os_ops;

	return register_os(default_os);
}

subsys_initcall(mvisor_os_init);
