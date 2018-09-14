#ifndef _MINOS_PRINT_H_
#define _MINOS_PRINT_H_

void log_init(void);

int level_print(char *fmt, ...);

#define	pr_debug(...)	level_print("[00 4] : " __VA_ARGS__)
#define pr_info(...)	level_print("[00 3] : " __VA_ARGS__)
#define pr_warn(...)	level_print("[00 2] : " __VA_ARGS__)
#define pr_error(...)	level_print("[00 1] : " __VA_ARGS__)
#define pr_fatal(...)	level_print("[00 0] : " __VA_ARGS__)

#define printf(...)	level_print("[00 0] : " __VA_ARGS__)

#endif
