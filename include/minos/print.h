#ifndef _MINOS_PRINT_H_
#define _MINOS_PRINT_H_

#include <config/config.h>

#define PRINT_LEVEL_FATAL	0
#define PRINT_LEVEL_ERROR	1
#define PRINT_LEVEL_WARN	2
#define PRINT_LEVEL_NOTICE	3
#define PRINT_LEVEL_INFO	4
#define PRINT_LEVEL_DEBUG	5

int level_print(int level, char *fmt, ...);
void change_log_level(unsigned int level);
int printf(char *fmt, ...);

#ifdef CONFIG_LOG_LEVEL_COLORFUL
#define PRINT_COLOR_RESET   "\e[m"
#define PRINT_COLOR_REVERSE "\e[7m"
#define PRINT_COLOR_RED     "\e[31m"
#define PRINT_COLOR_GREEN   "\e[32m"
#define PRINT_COLOR_YELLOW  "\e[33m"
#define PRINT_COLOR_BLUE    "\e[34m"
#else
#define PRINT_COLOR_RESET
#define PRINT_COLOR_REVERSE
#define PRINT_COLOR_RED
#define PRINT_COLOR_GREEN
#define PRINT_COLOR_YELLOW
#define PRINT_COLOR_BLUE
#endif

#define pr_debug(...)                                                          \
    level_print(PRINT_LEVEL_DEBUG,                                             \
                PRINT_COLOR_BLUE "DBG"                                         \
                PRINT_COLOR_RESET " " __VA_ARGS__)

#define pr_info(...)                                                           \
    level_print(PRINT_LEVEL_INFO,                                              \
                "INF"                                                          \
                PRINT_COLOR_RESET " " __VA_ARGS__)

#define pr_notice(...)                                                         \
    level_print(PRINT_LEVEL_NOTICE,                                            \
                PRINT_COLOR_GREEN "NIC"                                        \
                PRINT_COLOR_RESET " " __VA_ARGS__)

#define pr_warn(...)                                                           \
    level_print(PRINT_LEVEL_WARN,                                              \
                PRINT_COLOR_YELLOW "WRN"                                       \
                PRINT_COLOR_RESET " " __VA_ARGS__)

#define pr_err(...)                                                            \
    level_print(PRINT_LEVEL_ERROR,                                             \
                PRINT_COLOR_RED "ERR"                                          \
                PRINT_COLOR_RESET " " __VA_ARGS__)

#define pr_fatal(...)                                                          \
    level_print(PRINT_LEVEL_FATAL,                                             \
                PRINT_COLOR_REVERSE PRINT_COLOR_RED "FAT"                      \
                PRINT_COLOR_RESET " " __VA_ARGS__)

#endif
