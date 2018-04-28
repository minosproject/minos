#ifndef _MVISOR_PRINT_H_
#define _MVISOR_PRINT_H_

void mvisor_log_init(void);

int level_print(const char *fmt, ...);

#define	pr_debug(...)		level_print("4DEBUG : " __VA_ARGS__)
#define pr_info(...)		level_print("3INFO  : " __VA_ARGS__)
#define pr_warning(...)		level_print("2WARN  : " __VA_ARGS__)
#define pr_error(...)		level_print("1ERROR : " __VA_ARGS__)
#define pr_fatal(...)		level_print("0FATAL : " __VA_ARGS__)

#endif
