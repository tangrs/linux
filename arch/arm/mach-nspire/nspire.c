#include <linux/init.h>
#include <linux/io.h>
#include <asm/mach/nspire-regs.h>
#include <asm/mach-types.h>

#include "irq.h"

static struct map_desc nspire_io_regs[] __initdata = {
	{
		.virtual	= NSPIRE_APB_VIRT_BASE,
		.pfn		= __phys_to_pfn(NSPIRE_APB_PHYS_BASE),
		.length		= NSPIRE_APB_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= NSPIRE_VIC_VIRT_BASE,
		.pfn		= __phys_to_pfn(NSPIRE_VIC_PHYS_BASE),
		.length		= NSPIRE_VIC_SIZE,
		.type		= MT_DEVICE,
	}
};

static struct clk sp804_clk = {
	.rate	= 1000000,
};

static struct clk_lookup clocks[] = {
    {
        .con_id = "sp804",
        .clk = &sp804_clk
    }
};

static struct sys_timer nspire_sys_timer = {
	.init		= nspire_timer_init,
};

void __init nspire_map_io(void)
{
	iotable_init(nspire_io_regs, ARRAY_SIZE(nspire_io)));
}

void __init nspire_init_irq(void)
{
	vic_init(IOMEM(NSPIRE_VIC_VIRT_BASE), 0, NSPIRE_IRQ_MASK, 0);
}

void __init nspire_init_early(void)
{
    clkdev_add_table(lookups, ARRAY_SIZE(clocks));
}

void __init nspire_timer_init(void)
{
	sp804_clockevents_init(IOMEM(NSPIRE_TIMER2_VIRT_BASE), timer_irq, "timer2");
}

void __init nspire_restart(void)
{
	while (1) {
	    writel(2, IOMEM(NSPIRE_POWER_VIRT_BASE + 0x8));
	}
}

void __init nspire_init_late(void)
{
}

static struct irqaction nspire_timer_irq = {
	.name		= "nspire second timer",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= nspire_timer_interrupt,
};

MACHINE_START(NSPIRE, "TI-NSPIRE CX Calculator")
	.map_io		= nspire_map_io,
	.init_irq	= nspire_init_irq,
	.handle_irq	= vic_handle_irq,
	.timer		= &nspire_sys_timer,
	.init_machine	= nspire_init_early,
	.init_late	= nspire_init_late,
	.restart	= nspire_restart,
MACHINE_END