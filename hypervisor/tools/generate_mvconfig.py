import sys
import os
import json


def parse_mem_sectype(data):
    d = data['sectype']
    if d is 'S':
        value = '0'
    elif d is 'N':
        value = '1'
    else:
        value = '2'

    return ".sectype = " + value + ",\n"


def parse_vcpu_affinity(data):
    nr = data['nr_vcpu']
    nr = int(nr)

    c = ".vcpu_affinity = {"
    for i in range(0, nr):
        key = "vcpu" + str(i) +"_affinity"
        if key in data.keys():
            c += str(data[key])
        else:
            c += '0'

        if i != (nr - 1):
            c += ', '

    c += '},\n'

    return c


patten = [
    {
        'struct_name': "vmtag",
        'member_name': 'vmtags',
        'json_name': 'vmtags',
        'type' : 'ARRAY',
        'static': True,
        'members': [
            {'name': 'vmid', 'type': 'NUM', 'data': None},
            {'name': 'name', 'type': 'STRING', 'data': None},
            {'name': 'type', 'type': 'STRING', 'data': None},
            {'name': 'nr_vcpu', 'type': 'NUM', 'data': None},
            {'name': 'entry', 'type': 'HEX_STRING', 'data': None},
            {'name': 'setup_data', 'type': 'HEX_STRING', 'data': None},
            {'name': 'cmdline', 'type': 'STRING', 'data': None},
            {'name': 'vcpu_affinity', 'type': 'FUNCTION', 'data': parse_vcpu_affinity},
            {'name': 'bit64', 'type': "NUM", 'data': None}
        ]
    },
    {
        'struct_name': 'irqtag',
        'member_name': 'irqtags',
        'type': 'ARRAY',
        'static': True,
        'json_name': 'irqtags',
        'members': [
            {'name': 'vno', 'type': 'NUM', 'data': None},
            {'name': 'hno', 'type': 'NUM', 'data': None},
            {'name': 'vmid', 'type': 'NUM', 'data': None},
            {'name': 'vcpu_id', 'type': 'NUM', 'data': None},
            {'name': 'name', 'type': 'STRING', 'data': None}
        ]
    },
    {
        'struct_name': 'memtag',
        'member_name': 'memtags',
        'json_name': "memtags",
        'type' : 'ARRAY',
        'static': True,
        'members' : [
            {'name': 'mem_base', 'type': 'HEX_STRING', 'data': None},
            {'name': 'mem_end', 'type': 'HEX_STRING', 'data': None},
            {'name': 'enable', 'type': 'NUM', 'data': None},
            {'name': 'type', 'type': 'NUM', 'data': None},
            {'name': 'vmid', 'type': 'NUM', 'data': None},
            {'name': 'name', 'type': 'STRING', 'data': None},
        ]
    },
    {
        'struct_name': "virt_config",
        'member_name': "virt_config",
        'json_name': None,
        'type': "NORMAL",
        'static': False,
        'members': [
            {'name': 'version', 'type': 'STRING', 'data': None},
            {'name': 'platform', 'type': "STRING", 'data': None},
            {'name': 'vmtags', 'type': "CONFIG", 'data': 0},
            {'name': 'irqtags', 'type': "CONFIG", 'data': 1},
            {'name': "memtags", 'type': "CONFIG", 'data': 2},
            {'name': 'nr_vmtag', 'type': "GET_STRUCT_SIZE", 'data': 0},
            {'name': 'nr_irqtag', 'type': "GET_STRUCT_SIZE", 'data': 1},
            {'name': "nr_memtag", 'type': "GET_STRUCT_SIZE", 'data': 2}
        ]
    }

]


def parse_data(source, t, arg):
    if t == 'NUM':
        if type(source) is int:
            value = source
        elif type(source) is str:
            value = int(source)
        else:
            value = 0
        value = str(value)
    elif t == 'STRING':
        if (type(source)) is str:
            value = "\"" + source + "\""
        else:
            value = "\"" + str(source) + "\""
    elif t == "HEX_STRING":
        if (type(source)) is str:
            value = source
        else:
            value = "0x0"
    elif t == 'SIZE_IN_XB':
        v = int(source[:-2])
        if source[-2] is 'K':
            v = v * 1024
        elif source[-2] is 'M':
            v = v * 1024 * 1024
        elif source[-2] is 'G':
            v = v * 1024 * 1024 * 1024
        else:
            v = 0
        value = str(v)
    else:
        prinf("unsupport type")
        return ""

    return value


def parse_one_member(member_items, data):
    c = ''
    for m in member_items:
        if m['type'] is 'FUNCTION':
            value = m['data'](data)
        elif m['type'] is 'GET_STRUCT_SIZE':
            index = m['data']
            index = int(index)
            value = "sizeof(" + patten[index]['member_name'] + ') / sizeof(struct ' + patten[index]['struct_name'] + ')'
        elif m['type'] is 'CONFIG':
            index = m['data']
            index = int(index)
            value = patten[index]['member_name']
            if patten[index]['type'] is 'NORMAL':
                value = '&' + value
        else:
            d = data[m['name']]
            value = parse_data(d, m['type'], m['data'])

        if m['type'] is 'FUNCTION':
            c += "                " + value
        else:
            c += "                ." + m['name'] + " = " + value + ',\n'


    return c


def parse_struct(struct, jdata):
    c = ""
    if struct['static']:
        c += 'static '

    c += "struct " + struct['struct_name'] + ' __section(.__config) ' + struct['member_name']

    if struct['type'] is 'ARRAY':
        c += '[] '

    c += ' = {\n'

    if struct['type'] == "ARRAY":
        for data in jdata:
            s = '        {\n'
            s += parse_one_member(struct['members'], data)
            s += '        },\n'
            c += s
    else:
            c += parse_one_member(struct['members'], jdata)

    #if struct['type'] == "ARRAY":
    #    c += '        { }\n'

    c += '};\n\n'

    return c


def generate_mvconfig(json_data):
    content = "#include <minos/compiler.h>\n\n"
    content += "#include <minos/virt.h>\n"

    for struct in patten:
        if struct['json_name']:
            jdata = json_data[struct['json_name']]
        else:
            jdata = json_data
        print("Parsing " + struct['struct_name'] + " ...")
        content += parse_struct(struct, jdata)

    return content


"""
arg[0] is the tools name
arg[1] is the input file
arg[2] is the output file
"""
if __name__ == "__main__":
    argv = sys.argv
    if len(argv) < 3:
        print("argument is wrong")
        exit()

    input_name = argv[1]
    output_name = argv[2]

    with open(input_name, 'r') as fd:
        mvconfig = json.load(fd)
        content = generate_mvconfig(mvconfig)
        fd.close()

    with open(output_name, 'w') as fd:
        fd.write(content)
        fd.close()

    print("generate the config file success")
    exit()
