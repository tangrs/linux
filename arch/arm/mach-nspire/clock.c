#include <mach/clkdev.h>

void clk_disable(struct clk *clk) { }
int clk_enable(struct clk *clk) { return 0; }
unsigned long clk_get_rate(struct clk *clk) {
    return clk->rate;
}