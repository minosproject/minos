#ifndef __MVM_XNU_H__
#define __MVM_XNU_H__

#include <sys/types.h>
#include <inttypes.h>

#define XNU_ARM64_KBOOT_ARGS_REVISION2	2
#define XNU_ARM64_KBOOT_ARGS_VERSION	2
#define XNU_ARM64_BOOT_LINE_LENGTH	256

#define LC_SEGMENT_64			0x19
#define LC_UNIXTHREAD			0x5

#define XNU_DT_PROP_NAME_LENGTH		32

struct xnu_dt_node_prop {
	char name[XNU_DT_PROP_NAME_LENGTH];
	uint32_t length;
	char value[0];
};

struct segment_cmd64 {
	uint32_t cmd;
	uint32_t cmdsize;
	char seg_name[16];
	uint64_t vm_addr;
	uint64_t vm_size;
	uint64_t file_off;
	uint64_t file_size;
	uint32_t max_port;
	uint32_t init_port;
	uint32_t nsects;
	uint32_t flags;
};

struct mach_hdr64 {
	uint32_t magic;			/* mach magic number */
	uint32_t cpu_type;		/* cpu type */
	uint32_t cpu_sub_type;		/* cpu sub type */
	uint32_t file_type;		/* type of file */
	uint32_t nr_cmds;		/* number of olad commands */
	uint32_t size_of_cmds;		/* the size of all the load commands */
	uint32_t flags;			/* flags */
	uint32_t reserved;		/* resered */
};

struct load_cmd {
	uint32_t cmd;			/* type of load command */
	uint32_t cmd_size;		/* total size of command in bytes */
};

struct xnu_arm64_boot_video {
	unsigned long v_base_addr;	/* base address of video memory */
	unsigned long v_display;	/* display code */
	unsigned long v_row_bytes;	/* number of bytes per pixel row */
	unsigned long v_width;		/* width */
	unsigned long v_height;		/* height */
	unsigned long v_depth;		/* pixel depth and other parameters */
};

struct xnu_arm64_monitor_boot_args {
	uint64_t version;		 /* structure version - this is version 2 */
	uint64_t virt_base;              /* virtual base of memory assigned to the monitor */
	uint64_t phys_base;              /* physical address corresponding to the virtual base */
	uint64_t mem_size;               /* size of memory assigned to the monitor */
	uint64_t kern_args;              /* physical address of the kernel boot_args structure */
	uint64_t kern_entry;             /* kernel entrypoint */
	uint64_t kern_phys_base;	 /* physical base of the kernel's address space */
	uint64_t kern_phys_slide;        /* offset from kernPhysBase to kernel load address */
	uint64_t kern_virt_slide;        /* virtual slide applied to kernel at load time */
};

struct xnu_arm64_boot_args {
	uint16_t revision;		/* Revision of boot_args structure */
	uint16_t version;		/* Version of boot_args structure */
	uint64_t virt_base;             /* Virtual base of memory */
	uint64_t phys_base;             /* Physical base of memory */
	uint64_t mem_size;              /* Size of memory */
	uint64_t top_of_kdata;          /* Highest physical address used in kernel data area */
	struct xnu_arm64_boot_video video; /* Video Information */
	uint32_t machine_type;          /* Machine Type */
	uint64_t dtb;			/* Base of flattened device tree */
	uint32_t dtb_length;		/* Length of flattened tree */
	char cmdline[XNU_ARM64_BOOT_LINE_LENGTH]; /* Passed in command line */
	uint64_t boot_flags;            /* Additional flags specified by the bootloader */
	uint64_t mem_size_actual;       /* Actual size of memory */
};

#endif
