#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <inttypes.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/ether.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/epoll.h>
#include <linux/netlink.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <mvm/mvm.h>

static int os_setup_env(struct vm *vm)
{
	return 0;
}

static int os_load_image(struct vm *vm)
{
	return 0;
}

static int os_early_init(struct vm *vm)
{
	return 0;
}

struct vm_os os_other = {
	.name		= "unknown",
	.early_init	= os_early_init,
	.load_image	= os_load_image,
	.setup_vm_env   = os_setup_env,
};
