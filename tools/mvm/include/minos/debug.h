#ifndef __DEBUG_H__
#define __DEBUG_H__

extern int debug_enable;
extern int stdio_in_use;

#define pr_debug(...)					\
	do {						\
		if (debug_enable && !stdio_in_use)	\
			printf("[DEBUG] " __VA_ARGS__); \
	} while (0)

#define pr_err(...)					\
	do {						\
		if (!stdio_in_use)			\
			printf("[ERROR] " __VA_ARGS__); \
	} while (0)

#define pr_notice(...)					\
	do {						\
		if (!stdio_in_use)			\
			printf("[NIC  ] " __VA_ARGS__); \
	} while (0)

#define pr_info(...)					\
	do {						\
		if (!stdio_in_use)			\
			printf("[INFO ] " __VA_ARGS__); \
	} while (0)

#define pr_warn(...)					\
	do {						\
		if (!stdio_in_use)			\
			printf("[WARN ] " __VA_ARGS__); \
	} while (0)

#endif
