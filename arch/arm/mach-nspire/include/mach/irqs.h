#ifndef NSPIRE_IRQS_H
#define NSPIRE_IRQS_H

#define NSPIRE_IRQ_MASK         (1<<19) //0x006FEB9A

enum {
    NSPIRE_IRQ_UART = 1,
    NSPIRE_IRQ_TIMER2 = 19
};

#define NR_IRQS 64

#endif