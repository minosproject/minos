#ifndef _PRINTK_H
#define _PRINTK_H

int level_print(const char *fmt, ...);

#define	pr_debug(fmt, ...)		level_print("4" "DEBUG" fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)		level_print("3" "INFO" fmt, ##__VA_ARGS__)
#define pr_warning(fmt, ...)		level_print("2" "WARN" fmt, ##__VA_ARGS__)
#define pr_error(fmt, ...)		level_print("1" "ERROR" fmt, ##__VA_ARGS__)
#define pr_fatal(fmt, ...)		level_print("0" "FATAL" fmt, ##__VA_ARGS__)

#endif
