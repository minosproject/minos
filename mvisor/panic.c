/*
 * Created by Le Min 2017/12/12
 */

#include <mvisor/mvisor.h>

void panic(char *str)
{
	pr_fatal("--------- PANIC -------\n");
	pr_fatal("%s\n", str);
	pr_fatal("--------- PANIC END -------\n");
	while (1);
}
