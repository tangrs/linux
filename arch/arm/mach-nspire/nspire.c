#include <linux/init.h>
#include <linux/io.h>
#include <linux/clkdev.h>
#include <mach/nspire-regs.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/irqs.h>
#include <mach/clkdev.h>
#include <asm/mach/time.h>
#include <asm/hardware/timer-sp.h>
#include <asm/hardware/vic.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>



/**************** MAPIO ****************/
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

void __init nspire_map_io(void)
{
	iotable_init(nspire_io_regs, ARRAY_SIZE(nspire_io_regs));
}


/**************** IRQ ****************/
void __init nspire_init_irq(void)
{
	vic_init(IOMEM(NSPIRE_VIC_VIRT_BASE), 0, NSPIRE_IRQ_MASK, 0);
}

/**************** UART **************/

static struct resource uart_resources[] = {
    {
        .start = NSPIRE_APB_VIRT(NSPIRE_APB_UART),
        .end   = NSPIRE_APB_VIRT(NSPIRE_APB_UART + SZ_4K),
        .flags = IORESOURCE_MEM
    },
    {
		.start		= NSPIRE_IRQ_UART,
		.end		= NSPIRE_IRQ_UART,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device uart = {
    .name = "uart-pl011",
    .num_resources = ARRAY_SIZE(uart_resources),
    .resource = uart_resources
};


/*
static struct irqaction nspire_timer_irq = {
	.name		= "nspire second timer",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= nspire_timer_interrupt,
};
*/


/**************** TIMER ****************/
static struct clk sp804_clk = {
	.rate	= 32000,
};

static struct clk_lookup lookup = {
    .dev_id = "sp804",
    .con_id = "timer2",
    .clk = &sp804_clk
};

void __init nspire_timer_init(void)
{
	sp804_clockevents_init(NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2), NSPIRE_IRQ_TIMER2, "timer2");
}

static struct sys_timer nspire_sys_timer = {
	.init		= nspire_timer_init,
};


/************** INIT ***************/

void __init nspire_init_early(void){
    clkdev_add_table(&lookup, 1);
}

void __init nspire_init(void)
{
    platform_device_register(&uart);
}

void __init nspire_restart(char mode, const char *cmd)
{
	while (1) {
	    writel(2, NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x8));
	}
}

void __init nspire_init_late(void)
{
}

MACHINE_START(NSPIRE, "TI-NSPIRE CX Calculator")
	.map_io		= nspire_map_io,
	.init_irq	= nspire_init_irq,
	.handle_irq	= vic_handle_irq,
	.timer		= &nspire_sys_timer,
	.init_early     = nspire_init_early,
	.init_machine	= nspire_init,
	.init_late	= nspire_init_late,
	.restart	= nspire_restart,
MACHINE_END