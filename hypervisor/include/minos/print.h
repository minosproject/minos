#ifndef _MINOS_PRINT_H_
#define _MINOS_PRINT_H_

void log_init(void);

int level_print(char *fmt, ...);

#ifndef CONFIG_LOG_LEVEL
#define CONFIG_LOG_LEVEL	3
#endif

#if (CONFIG_LOG_LEVEL == 4)
#define	pr_debug(...)	level_print("[00 4] : " __VA_ARGS__)
#define pr_info(...)	level_print("[00 3] : " __VA_ARGS__)
#define pr_warn(...)	level_print("[00 2] : " __VA_ARGS__)
#define pr_error(...)	level_print("[00 1] : " __VA_ARGS__)
#define pr_fatal(...)	level_print("[00 0] : " __VA_ARGS__)
#elif (CONFIG_LOG_LEVEL == 3)
#define	pr_debug(...)
#define pr_info(...)	level_print("[00 3] : " __VA_ARGS__)
#define pr_warn(...)	level_print("[00 2] : " __VA_ARGS__)
#define pr_error(...)	level_print("[00 1] : " __VA_ARGS__)
#define pr_fatal(...)	level_print("[00 0] : " __VA_ARGS__)
#elif (CONFIG_LOG_LEVEL == 2)
#define	pr_debug(...)
#define pr_info(...)
#define pr_warn(...)	level_print("[00 2] : " __VA_ARGS__)
#define pr_error(...)	level_print("[00 1] : " __VA_ARGS__)
#define pr_fatal(...)	level_print("[00 0] : " __VA_ARGS__)
#elif (CONFIG_LOG_LEVEL == 1)
#define	pr_debug(...)
#define pr_info(...)
#define pr_warn(...)
#define pr_error(...)	level_print("[00 1] : " __VA_ARGS__)
#define pr_fatal(...)	level_print("[00 0] : " __VA_ARGS__)
#else
#define	pr_debug(...)
#define pr_info(...)
#define pr_warn(...)
#define pr_error(...)
#define pr_fatal(...)	level_print("[00 0] : " __VA_ARGS__)
#endif

#define printf(...)	level_print("[00 3] : " __VA_ARGS__)

#endif
