#ifndef __MINOS_COMMAND_H__
#define __MINOS_COMMAND_H__

struct shell_command {
	int min_args;
	char *name;
	char *cmd_info;
	int (*hdl)(int argc, char **argv);
};

#define DEFINE_SHELL_COMMAND(a_name, c_name, c_cmd_info, c_hdl, args)	\
	static struct shell_command __used				\
	shell_command_##a_name __section(.__shell_command) = {		\
		.min_args	= args,					\
		.name		= c_name,				\
		.cmd_info	= c_cmd_info,				\
		.hdl		= c_hdl,				\
	}

int excute_shell_command(int argc, char **argv);

int shell_task(void *data);

#endif
