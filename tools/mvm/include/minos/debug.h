#ifndef __DEBUG_H__
#define __DEBUG_H__

extern int debug_enable;

#define pr_debug(...)	\
	do {				\
		if (debug_enable)	\
			printf("[DEBUG] " __VA_ARGS__); \
	} while (0)

#define pr_err(...)	printf("[ERROR] " __VA_ARGS__)
#define pr_notice(...)	printf("[NIC  ] " __VA_ARGS__)
#define pr_info(...)	printf("[INFO ] " __VA_ARGS__)
#define pr_warn(...)	printf("[WARN ] " __VA_ARGS__)

#endif
