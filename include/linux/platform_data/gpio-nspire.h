/*
 * gpio-nspire platform data struct
 *
 * Author: Fabian Vogt <fabian@ritter-vogt.de>
 */

#ifndef __GPIO_NSPIRE_H__
#define __GPIO_NSPIRE_H__

#include <linux/mtd/nand.h>


struct nspire_gpio_data {
	unsigned	irq_base;
	unsigned	ngpio;
};

#endif /* __GPIO_NSPIRE_H__ */
