#ifndef _PRINTK_H
#define _PRINTK_H

int level_print(const char *fmt, ...);

#define	pr_debug(...)		level_print("4DEBUG : " __VA_ARGS__)
#define pr_info(...)		level_print("3INFO  : " __VA_ARGS__)
#define pr_warning(...)		level_print("2WARN  : " __VA_ARGS__)
#define pr_error(...)		level_print("1ERROR : " __VA_ARGS__)
#define pr_fatal(...)		level_print("0FATAL : " __VA_ARGS__)

#endif
