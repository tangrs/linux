/*
 *  linux/drivers/clocksource/nspire-classic-timer.c
 *
 *  Copyright (C) 2013 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <linux/slab.h>

#define DT_COMPAT	"nspire-classic-timer"

#define IO_CURRENT_VAL	0x00
#define IO_DIVIDER	0x04
#define IO_CONTROL	0x08

#define IO_TIMER1	0x00
#define IO_TIMER2	0x0C

#define IO_MATCH1	0x18
#define IO_MATCH2	0x1C
#define IO_MATCH3	0x20
#define IO_MATCH4	0x24
#define IO_MATCH5	0x28
#define IO_MATCH6	0x2C

#define IO_INTR_STS	0x00
#define IO_INTR_ACK	0x00
#define IO_INTR_MSK	0x04

#define CNTL_STOP_TIMER	(1<<4)
#define CNTL_RUN_TIMER	(0<<4)

#define CNTL_INC	(1<<3)
#define CNTL_DEC	(0<<3)

#define CNTL_TOZERO	0
#define CNTL_MATCH1	1
#define CNTL_MATCH2	2
#define CNTL_MATCH3	3
#define CNTL_MATCH4	4
#define CNTL_MATCH5	5
#define CNTL_MATCH6	6
#define CNTL_FOREVER	7

struct nspire_timer {
	void __iomem *base;
	void __iomem *timer1, *timer2;
	void __iomem *interrupt_regs;

	int irqnr;

	struct clk *clk;
	struct clock_event_device clkevt;
	struct irqaction clkevt_irq;

	char clocksource_name[64];
	char clockevent_name[64];
};

static int nspire_timer_set_event(unsigned long delta,
		struct clock_event_device *dev)
{
	unsigned long flags;
	struct nspire_timer *timer = container_of(dev,
			struct nspire_timer,
			clkevt);

	local_irq_save(flags);

	writel(delta, timer->timer1 + IO_CURRENT_VAL);
	writel(CNTL_RUN_TIMER | CNTL_DEC | CNTL_MATCH1,
			timer->timer1 + IO_CONTROL);

	local_irq_restore(flags);

	return 0;
}
static void nspire_timer_set_mode(enum clock_event_mode mode,
		struct clock_event_device *evt)
{
	evt->mode = mode;
}

static irqreturn_t nspire_timer_interrupt(int irq, void *dev_id)
{
	struct nspire_timer *timer = dev_id;

	writel((1<<0), timer->interrupt_regs + IO_INTR_ACK);
	writel(CNTL_STOP_TIMER, timer->timer1 + IO_CONTROL);

	if (timer->clkevt.event_handler)
		timer->clkevt.event_handler(&timer->clkevt);

	return IRQ_HANDLED;
}

static int __init nspire_timer_add(struct device_node *node)
{
	struct nspire_timer *timer;
	struct resource res;
	int ret;

	timer = kzalloc(sizeof(*timer), GFP_ATOMIC);
	if (!timer)
		return -ENOMEM;

	timer->base = of_iomap(node, 0);
	if (!timer->base) {
		ret = -EINVAL;
		goto error_free;
	}
	timer->timer1 = timer->base + IO_TIMER1;
	timer->timer2 = timer->base + IO_TIMER2;

	timer->clk = of_clk_get(node, 0);
	if (IS_ERR(timer->clk)) {
		ret = PTR_ERR(timer->clk);
		pr_err("Timer clock not found! (error %d)\n", ret);
		goto error_unmap;
	}

	timer->interrupt_regs = of_iomap(node, 1);
	timer->irqnr = irq_of_parse_and_map(node, 0);

	of_address_to_resource(node, 0, &res);
	scnprintf(timer->clocksource_name, sizeof(timer->clocksource_name),
			"%llx.%s_clocksource",
			(unsigned long long)res.start, node->name);

	scnprintf(timer->clockevent_name, sizeof(timer->clockevent_name),
			"%llx.%s_clockevent",
			(unsigned long long)res.start, node->name);

	if (timer->interrupt_regs && timer->irqnr) {
		timer->clkevt.name	= timer->clockevent_name;
		timer->clkevt.set_next_event = nspire_timer_set_event;
		timer->clkevt.set_mode	= nspire_timer_set_mode;
		timer->clkevt.rating	= 200;
		timer->clkevt.shift	= 32;
		timer->clkevt.cpumask	= cpu_all_mask;
		timer->clkevt.features	=
			CLOCK_EVT_FEAT_ONESHOT;

		writel(CNTL_STOP_TIMER, timer->timer1 + IO_CONTROL);
		writel(0, timer->timer1 + IO_DIVIDER);

		/* Mask out interrupts except the one we're using */
		writel((1<<0), timer->interrupt_regs + IO_INTR_MSK);
		writel(0x3F, timer->interrupt_regs + IO_INTR_ACK);

		/* Interrupt to occur when timer value matches 0 */
		writel(0, timer->base + IO_MATCH1);

		timer->clkevt_irq.name	= timer->clockevent_name;
		timer->clkevt_irq.handler = nspire_timer_interrupt;
		timer->clkevt_irq.dev_id = timer;
		timer->clkevt_irq.flags	=
			IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL;

		setup_irq(timer->irqnr, &timer->clkevt_irq);

		clockevents_config_and_register(&timer->clkevt,
				clk_get_rate(timer->clk), 0x0001, 0xfffe);
		pr_info("Added %s as clockevent\n", timer->clockevent_name);
	}

	writel(CNTL_STOP_TIMER, timer->timer2 + IO_CONTROL);
	writel(0, timer->timer2 + IO_CURRENT_VAL);
	writel(0, timer->timer2 + IO_DIVIDER);
	writel(CNTL_RUN_TIMER | CNTL_FOREVER | CNTL_INC,
			timer->timer2 + IO_CONTROL);

	clocksource_mmio_init(timer->timer2 + IO_CURRENT_VAL,
			timer->clocksource_name,
			clk_get_rate(timer->clk),
			200, 16,
			clocksource_mmio_readw_up);

	pr_info("Added %s as clocksource\n", timer->clocksource_name);

	return 0;
error_unmap:
	iounmap(timer->base);
error_free:
	kfree(timer);
	return ret;
}

CLOCKSOURCE_OF_DECLARE(nspire_classic_timer, DT_COMPAT, nspire_timer_add);
