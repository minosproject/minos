import sys
import os


"""
the default config for the hypervisor, user
can override them in the xxx_defconfig file
if the item did not mentioned in the xxx_defconfig
file, these config will use the config

format:
key:[value, keep_default]

"""
default_config = {
    'CONFIG_MAX_VM': ['64', 1],
    'CONFIG_MINOS_RESCHED_IRQ': ['7', 1],
    'CONFIG_MAX_SLAB_BLOCKS': ['10', 1],
    'CONFIG_PLATFORM_ADDRESS_RANGE': ['40', 1],
    'CONFIG_LOG_LEVEL': ['3', 1],
    'CONFIG_MINOS_START_ADDRESS': ['0x0', 1],
    'CONFIG_BOOTMEM_SIZE': ['64K', 1],
    'CONFIG_MAX_MAILBOX_NR': ['10', 1],
    'CONFIG_TASK_RUN_TIME' : ['100', 1],
}


def parse_line(s):
    return [i.strip('\r\n') for i in s.split('=') if i]


"""
arg[0] the tools name
arg[1] the input config file
arg[1] the directory the generated file stored to
"""
if __name__ == "__main__":
    argv = sys.argv
    if len(argv) < 3:
        print("argument is wrong")
        exit()

    input_name = argv[1]
    output_dir = argv[2]

    output_conf_file = output_dir + "/auto.conf"
    output_h_file = output_dir + "/config.h"

    configs = []

    with open(input_name, 'r') as fd:
        for line in fd:
            if not line:
                continue
            if line[0] == '#' or line[0] == ' ' or line[0] == '':
                continue
            if line[0] == '\r' or line[0] == '\n':
                continue

            item = parse_line(line)
            configs.append(item)

            # check whether need to change the default config
            if item[0] in default_config.keys():
                default_config[item[0]][1] = 0

        fd.close()

    print("")
    for key in default_config.keys():
        if default_config[key][1]:
            configs.append([key, default_config[key][0]])

    a_str = ''

    with open(output_conf_file, 'w') as afd:
        for item in configs:
            if len(item) != 2:
                print("Error config item" + item)
                continue

            last_char = item[1][-1]
            a_str += item[0] + '='
            if last_char == 'M' or last_char == 'm':
                num = int(item[1][:-1]) * 1024 * 1024
                a_str += hex(num)
            elif last_char == 'K' or last_char == 'k':
                num = int(item[1][:-1]) * 1024
                a_str += hex(num)
            else:
                a_str += item[1]
            a_str += '\r\n'
        print(a_str)
        afd.write(a_str)
        afd.close()

    h_str = '#ifndef __MINOS_CONFIG_H__\r\n'
    h_str += "#define __MINOS_CONFIG_H__\r\n\r\n"
    with open(output_h_file, 'w') as hfd:
        for item in configs:
            if len(item) != 2:
                print("Error config item" + item)
                continue

            last_char = item[1][-1]
            h_str += '#define ' + item[0] + ' '
            if item[1] == 'y':
                h_str += '1'
            elif last_char == 'M' or last_char == 'm':
                num = int(item[1][:-1]) * 1024 * 1024
                h_str += hex(num)
            elif last_char == 'K' or last_char == 'k':
                num = int(item[1][:-1]) * 1024
                h_str += hex(num)
            else:
                h_str += item[1]
            h_str += '\r\n'
        h_str += '\r\n#endif'
        hfd.write(h_str)
        hfd.close()

        exit()
