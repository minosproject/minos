#ifndef _MINOS_PRINT_H_
#define _MINOS_PRINT_H_

#define PRINT_LEVEL_FATAL	0
#define PRINT_LEVEL_ERROR	1
#define PRINT_LEVEL_WARN	2
#define PRINT_LEVEL_NOTICE	3
#define PRINT_LEVEL_INFO	4
#define PRINT_LEVEL_DEBUG	5

int level_print(int level, char *fmt, ...);
void change_log_level(unsigned int level);

#define pr_debug(...)	level_print(PRINT_LEVEL_DEBUG, "DBG " __VA_ARGS__)
#define pr_info(...)	level_print(PRINT_LEVEL_INFO,  "INF " __VA_ARGS__)
#define pr_notice(...)	level_print(PRINT_LEVEL_NOTICE,"NIC " __VA_ARGS__)
#define pr_warn(...)	level_print(PRINT_LEVEL_WARN,  "WRN " __VA_ARGS__)
#define pr_err(...)	level_print(PRINT_LEVEL_ERROR, "ERR " __VA_ARGS__)
#define pr_fatal(...)	level_print(PRINT_LEVEL_FATAL, "FAT " __VA_ARGS__)
#define printf(...)	level_print(PRINT_LEVEL_INFO,  "INF " __VA_ARGS__)

#endif
