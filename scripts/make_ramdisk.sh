#!/bin/bash

# SPDX-License-Identifier: ISC

set -e -o pipefail

MAGIC_SIZE=16
SB_SIZE=$(((32 + 32 + 64 + 64) / 8))
FNAME_SIZE=32
INODE_SIZE=$((FNAME_SIZE + (64 + 64) / 8))

print_help() {
    cat << EOF
Usage: $(basename ${0}) -o IMAGE -- [FILE]...
Pack FILE(s) into IMAGE as Minos ramdisk.
Examples:
    # Pack list of files into ramdisk
    $(basename ${0}) -o IMAGE -- KERNEL FDT
    # Pack top-level files in directory into ramdisk
    $(basename ${0}) -o IMAGE -- DIR/*
EOF
}

# ${1}: bit
# ${2}: number
encode_number() {
    local len=$((${1} / 8 * 2))
    local hex=$(printf "%0${len}x" ${2})

    local i=${len}
    while ((i > 0)); do
        i=$((i - 2))
        printf "\x${hex:i:2}"
    done
}

# ${1}: file
pack_file() {
    printf "%s" $(basename ${1}) |
        dd conv=notrunc obs=${off1} of=${image} seek=1 2> /dev/null

    local size=$(stat -c "%s" ${1})
    {
        encode_number 64 ${off2}
        encode_number 64 ${size}
    } |
        dd conv=notrunc obs=$((off1 + FNAME_SIZE)) of=${image} seek=1 2> /dev/null

    dd conv=notrunc if=${1} obs=${off2} of=${image} seek=1 2> /dev/null

    off1=$((off1 + INODE_SIZE))
    off2=$((off2 + size))
}

make_ramdisk() {
    {
        printf "MINOSRAMDISK...."
        encode_number 32 ${#files[@]}
        encode_number 32 0
        encode_number 64 $((MAGIC_SIZE + SB_SIZE))
        encode_number 64 0
    } |
        dd conv=notrunc of=${image} 2> /dev/null

    local file
    for file in ${files[@]}; do
        printf "Packing %.*s\n" ${FNAME_SIZE} $(basename ${file})
        pack_file ${file}
    done

    printf "Done\n"
}

filter_files() {
    local i
    for i in ${!files[@]}; do
        if [[ ! -f ${files[i]} ]]; then
            unset files[i]
        fi
    done
}

# files: input files
# image: output image
# off1: inode offset
# off2: file offset
main() {
    while (($# > 0)); do
        case ${1} in
        -o)
            image=${2}
            shift
            shift || true
            ;;
        --)
            shift
            break
            ;;
        *)
            printf "Invalid option '%s'\n" ${1}
            print_help
            exit 2
            ;;
        esac
    done

    if [[ -z ${image} ]]; then
        printf "Missing option '-o'\n"
        print_help
        exit 2
    fi

    files=($@)
    filter_files

    off1=$((MAGIC_SIZE + SB_SIZE))
    off2=$((MAGIC_SIZE + SB_SIZE + INODE_SIZE * ${#files[@]}))

    rm -f ${image}
    make_ramdisk
}

main $@
