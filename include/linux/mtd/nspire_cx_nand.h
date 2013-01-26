/*
 *  linux/include/linux/mtd/nspire_cx_nand.h
 *
 *  Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#ifndef __NSPIRE_CX_NAND_H
#define __NSPIRE_CX_NAND_H

struct nspire_cx_nand_platdata {
	struct mtd_partition *partitions;
	int nr_partitions;
};

#endif
