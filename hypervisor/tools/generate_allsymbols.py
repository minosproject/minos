import sys
import os


if __name__ == "__main__":
    argv = sys.argv
    if len(argv) < 3:
        print("argument is wrong")
        exit()

    input_name = argv[1]
    output_name = argv[2]
    symbols = "#include <config/config.h>\n"
    symbols += "#ifdef ARCH_AARCH64\n"
    symbols += "#define PTR .quad\n"
    symbols += "#define ALGN .align 8\n"
    symbols += "#else\n"
    symbols += "#define PTR .long\n"
    symbols += "#define ALGN .align 4\n"
    symbols += "#endif\n\n"
    symbols += ".section .__symbols__, \"a\"\n"
    symbols += "        PTR        allsyms_addresses\n"
    symbols += "        PTR        allsyms_num_syms\n"
    symbols += "        PTR        allsyms_offset\n"
    symbols += "        PTR        allsyms_names\n\n"

    addrs = ".section .rodata, \"a\"\n"
    addrs += ".global allsyms_addresses\n        ALGN\n"
    addrs += "allsyms_addresses:\n"

    names = ".global allsyms_names\n        ALGN\n"
    names += "allsyms_names:\n"

    offsets = ".global allsyms_offset\n        ALGN\n"
    offsets += "allsyms_offset:\n"

    total_nums_chr = 0
    total_nums_syms = 0

    with open(input_name, 'r') as fd:
        line = fd.readline()
        while line :
            array = [i.strip('\r\t\n') for i in line.split(' ') if i]
            t = array[1]
            if t is 'T' or t is 't':
                total_nums_syms += 1
                addrs += "        PTR        0x%s\n" %(array[0])
                offsets += "        .long 0x%x\n" %(total_nums_chr)
                names += "        .byte "
                for letter in array[2]:
                    names += "0x%x, " %(ord(letter))
                    total_nums_chr += 1
                names += "0x0\n"
                total_nums_chr += 1
            line = fd.readline()
        fd.close()

    addrs += ".global allsyms_num_syms\n        ALGN\n"
    addrs += "allsyms_num_syms:\n"
    addrs += "        .long    0x%x\n\n" %(total_nums_syms)

    with open(output_name, 'w') as ofd:
        ofd.write(symbols)
        ofd.write(addrs)
        ofd.write(offsets)
        ofd.write(names)

    exit()
