#ifndef _PRINTK_H
#define _PRINTK_H

typedef void (*early_printk_t)(char *str);

int level_printk(const char *fmt, ...);

#define	pr_debug(module, fmt, ...)		level_printk("4" module fmt, ##__VA_ARGS__)
#define pr_info(module, fmt, ...)		level_printk("3" module fmt, ##__VA_ARGS__)
#define pr_warning(module, fmt, ...)		level_printk("2" module fmt, ##__VA_ARGS__)
#define pr_error(module, fmt, ...)		level_printk("1" module fmt, ##__VA_ARGS__)
#define pr_fatal(module, fmt, ...)		level_printk("0" module fmt, ##__VA_ARGS__)

#define kernel_debug(fmt, ...)			pr_debug("[ KERN: ]", fmt, ##__VA_ARGS__)
#define kernel_info(fmt, ...)			pr_info("[ KERN: ]", fmt, ##__VA_ARGS__)
#define kernel_warning(fmt, ...)		pr_warning("[ KERN: ]", fmt, ##__VA_ARGS__)
#define kernel_error(fmt, ...)			pr_error("[ KERN: ]", fmt, ##__VA_ARGS__)
#define kernel_fatal(fmt, ...)			pr_fatal("[ KERN: ]", fmt, ##__VA_ARGS__)

#define printk	kernel_info

#endif
