/*
 *  linux/drivers/mtd/nand/nspire_nand.c
 *
 *  Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>

union nspire_nand_addr {
	unsigned long addr;
	struct {
		unsigned __reserved0:3;
		unsigned start_cmd:8;
		unsigned end_cmd:8;
		unsigned is_data:1;
		unsigned end_cmd_valid:1;
		unsigned addr_cycles:3;
		unsigned chip_addr:8;
	} cmd;
	struct {
		unsigned __reserved0:10;
		unsigned ecc_last:1;
		unsigned end_cmd:8;
		unsigned is_data:1;
		unsigned end_cmd_valid:1;
		unsigned clear_cs:1;
		unsigned __reserved1:2;
		unsigned chip_addr:8;
	} data;
};

struct nspire_nand {
	struct nand_chip	chip;
	struct mtd_info		mtd;
	struct {
		unsigned long	addr;
		unsigned char	cmd;
		unsigned char	addr_cycles:3;
		unsigned char	cle:1;
		unsigned char	ale:1;
	}			state;

	unsigned char		chip_addr;
};

static u8 nspire_read_byte(struct mtd_info * mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct nspire_nand *pdata = chip->priv;
	union nspire_nand_addr n;
	void __iomem * io;
	uint8_t data;

	n.addr = 0;
	n.data.is_data = 1;
	n.data.clear_cs = 1;
	n.data.chip_addr = pdata->chip_addr;

	io = ioremap(n.addr, sizeof(u8));
	data = readb(io);
	iounmap(io);

	return data;
}

static int nspire_write(struct mtd_info * mtd, u8 cmd,
		unsigned long data, unsigned long addr_cycles)
{
	struct nand_chip *chip = mtd->priv;
	struct nspire_nand *pdata = chip->priv;
	union nspire_nand_addr n;
	void __iomem * io;

	n.addr = 0;
	n.cmd.addr_cycles = addr_cycles;
	n.cmd.start_cmd = cmd;
	n.cmd.chip_addr = pdata->chip_addr;

	io = ioremap(n.addr, sizeof(unsigned long));
	writel(data, io);
	iounmap(io);

	return 0;
}

static void nspire_cmd_ctrl(struct mtd_info *mtd, int data, unsigned int ctrl)
{
	struct nand_chip *chip = mtd->priv;
	struct nspire_nand *pdata = chip->priv;

	if (ctrl & NAND_CLE) {
		pdata->state.cmd = (data & 0xff);
		pdata->state.addr_cycles = 0;
		pdata->state.addr = 0;
	} else if (ctrl & NAND_ALE) {
		BUG_ON(pdata->state.addr_cycles > 4);
		pdata->state.addr |= (data & 0xff)<<
				(8*pdata->state.addr_cycles);
		pdata->state.addr_cycles++;
	} else if (ctrl & NAND_NCE) {
		nspire_write(mtd, pdata->state.cmd, pdata->state.addr,
				pdata->state.addr_cycles);
	}
}

static int nspire_nand_probe(struct platform_device *pdev)
{
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	struct nspire_nand *pdata;
	void __iomem *io;
	union nspire_nand_addr nand_addr;
	int err;

	if (res->start & 0x00ffffff) {
		dev_err(&pdev->dev, "Invalid chip address");
		return -EINVAL;
	}

	if (resource_size(res) != 0x01000000) {
		dev_err(&pdev->dev, "Invalid chip size");
		return -EINVAL;
	}

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	if (!devm_request_mem_region(&pdev->dev, res->start, resource_size(res),
			dev_name(&pdev->dev))) {
		dev_err(&pdev->dev, "Cannot request memory region");
		return -EBUSY;
	}

	pdata->chip_addr = (res->start)>>24;
	pdata->chip.priv = pdata;
	pdata->mtd.priv = &pdata->chip;
	pdata->mtd.owner = THIS_MODULE;
	pdata->mtd.name = dev_name(&pdev->dev);

	//pdata->chip.read_byte = nspire_read_byte;
	//pdata->chip.write_buf = nspire_write_buf;
	pdata->chip.cmd_ctrl = nspire_cmd_ctrl;

	nand_addr.addr = 0;
	nand_addr.data.is_data = 1;
	nand_addr.data.clear_cs = 1;
	nand_addr.data.chip_addr = pdata->chip_addr;
	io = devm_ioremap(&pdev->dev,
			nand_addr.addr, sizeof(u32));

	if (!io) {
		dev_err(&pdev->dev, "Cannot map memory region");
		return -EINVAL;
	}

	pdata->chip.IO_ADDR_R = io;
	pdata->chip.IO_ADDR_W = io;

	if (nand_scan(&pdata->mtd, 1)) {
		dev_err(&pdev->dev, "NAND scan failed");
		return -ENXIO;
	}

	err = mtd_device_parse_register(&pdata->mtd, 0, NULL,
			NULL, 0);
	if (err) {
		dev_err(&pdev->dev, "MTD register device failed");
		nand_release(&pdata->mtd);
		return err;
	}

	platform_set_drvdata(pdev, pdata);

	return 0;
}

static int nspire_nand_remove(struct platform_device *pdev)
{
	struct nspire_nand *pdata = platform_get_drvdata(pdev);
	nand_release(&pdata->mtd);

	return 0;
}

static struct platform_driver nspire_nand_driver = {
	.probe	= nspire_nand_probe,
	.remove	= nspire_nand_remove,
	.driver	= {
		.name		= "nspire_nand",
		.owner		= THIS_MODULE,
	},
};

module_platform_driver(nspire_nand_driver);
