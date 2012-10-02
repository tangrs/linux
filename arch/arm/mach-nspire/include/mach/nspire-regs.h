// //(NSPIRE_VIC_VIRT_BASE - NSPIRE_UART_SIZE)
//
// #define NSPIRE_TIMER2_PHYS_BASE    0x900D0000
// #define NSPIRE_TIMER2_SIZE         0x10000
// #define NSPIRE_TIMER2_VIRT_BASE (NSPIRE_UART_VIRT_BASE - NSPIRE_TIMER2_SIZE)
//
// #define NSPIRE_POWER_PHYS_BASE     0x900B0000
// #define NSPIRE_POWER_SIZE         0x10000
// #define NSPIRE_POWER_VIRT_BASE (NSPIRE_TIMER2_VIRT_BASE - NSPIRE_POWER_SIZE)

#include <asm-generic/sizes.h>

/*
    Memory map:
    0xfee00000 - 0xff000000         0x90000000 - 0x90200000     (APB)
    0xfedff000 - 0xfee00000         0xdc000000 - 0xdc001000     (Interrupt Controller)

*/
#define NSPIRE_APB_PHYS_BASE        0x90000000
#define NSPIRE_APB_SIZE             SZ_2M
#define NSPIRE_APB_VIRT_BASE        0xfee00000

#define NSPIRE_APB_PHYS(x)          ((NSPIRE_APB_PHYS_BASE) + (x))
#define NSPIRE_APB_VIRT(x)          ((NSPIRE_APB_VIRT_BASE) + (x))
#define NSPIRE_APB_VIRTIO(x)        IOMEM(NSPIRE_APB_VIRT(x))


#define NSPIRE_VIC_PHYS_BASE        0xDC000000
#define NSPIRE_VIC_SIZE             SZ_4K
#define NSPIRE_VIC_VIRT_BASE        0xfedff000

#define NSPIRE_APB_TIMER2           0xD0000
#define NSPIRE_APB_POWER            0xB0000
#define NSPIRE_APB_UART             0x20000