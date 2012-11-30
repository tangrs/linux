/*
 *  linux/arch/arm/mach-nspire/boot1.c
 *
 *  Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */
#include <linux/proc_fs.h>

#include <mach/nspire_mmio.h>

#define BOOT1_PROCFS_NAME "boot1_rom"

static struct proc_dir_entry *boot1_proc_entry;

static int boot1_read(char *buf, char **data, off_t offset,
	int len, int *eof, void *privdata)
{
	if (offset < NSPIRE_BOOT1_SIZE) {
		*data = (char *)NSPIRE_BOOT1_VIRT_BASE + offset;
		return NSPIRE_BOOT1_SIZE - offset;
	} else {
		*eof = 1;
		return 0;
	}
}

int __init boot1_procfs_init(void)
{
	boot1_proc_entry = create_proc_entry(BOOT1_PROCFS_NAME, 0644, NULL);
	if (!boot1_proc_entry) {
		pr_alert("Error: Could not initialize /proc/%s\n",
		       BOOT1_PROCFS_NAME);
		return -ENOMEM;
	}

	boot1_proc_entry->read_proc = boot1_read;
	boot1_proc_entry->size      = NSPIRE_BOOT1_SIZE;
	pr_info("BOOT1 ROM mapped to /proc/%s\n", BOOT1_PROCFS_NAME);
	return 0;
}
