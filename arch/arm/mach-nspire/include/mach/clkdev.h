#ifndef NSPIRE_CLKDEV_H
#define NSPIRE_CLKDEV_H

struct clk {
         unsigned long           rate;
};

#define __clk_get(clk)	({ 1; })
#define __clk_put(clk)	do { } while (0)

#endif