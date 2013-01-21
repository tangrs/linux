/*
 * Driver for the ADC in TI NSPIRE calculators
 *
 * Author: Fabian Vogt <fabian@ritter-vogt.de>
 *
 * Licensed under the GPLv2 or later.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/completion.h>

#define OFFSET_INT_RAW_STATUS	0x004
#define OFFSET_INT_MASK		0x008
#define OFFSET_CHANNEL		0x100
#define OFFSET_CHANNEL_WIDTH	0x020
#define OFFSET_CHANNEL_CMD	0x000
#define OFFSET_CHANNEL_RAW	0x010

struct nspire_adc_state {
	void __iomem		*base;
	struct completion	completion;
};

static irqreturn_t nspire_adc_isr(int irq, void *dev_id)
{
	struct nspire_adc_state *state = (struct nspire_adc_state *) dev_id;

	/* Ack all channels (we only read one channel at a time) */
	writel(~0, state->base + OFFSET_INT_RAW_STATUS);

	complete(&state->completion);

	return IRQ_HANDLED;
}

static int nspire_adc_read_raw(struct iio_dev *iio_dev,
			       struct iio_chan_spec const *chan, int *val,
			       int *val2, long mask)
{
	struct nspire_adc_state *state = iio_priv(iio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&iio_dev->mlock);
		/* Start measurement */
		writel(1, state->base + OFFSET_CHANNEL
			+ chan->address
			+ OFFSET_CHANNEL_CMD);

		/* Wait until IRQ fired */
		wait_for_completion(&state->completion);

		/* Read value */
		*val = readl(state->base + OFFSET_CHANNEL
			+ chan->address
			+ OFFSET_CHANNEL_RAW);

		/* Channel 0 + 1 use different scale */
		if (chan->channel < 2)
			*val <<= 1;

		mutex_unlock(&iio_dev->mlock);
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		/*    310 units = 1V
		 * -> 0.000310 units = 1 microV
		 * -> 1/0.000310 * x = microV = 3226 * x */
		*val = 3226;
		*val2 = 0;
		return IIO_VAL_INT_PLUS_MICRO;
	}
	return -EINVAL;
}

#define NSPIRE_ADC_CHANNEL(nr) {			 \
	.type		= IIO_VOLTAGE,			 \
	.indexed	= 1,				 \
	.channel	= nr,				 \
	.address	= nr*OFFSET_CHANNEL_WIDTH,	 \
	.scan_index	= nr,				 \
	.info_mask	= IIO_CHAN_INFO_RAW_SEPARATE_BIT \
		| IIO_CHAN_INFO_SCALE_SHARED_BIT,	 \
	.scan_type	= {				 \
		.sign	= 'u',				 \
		.endianness = IIO_LE,			 \
	},						 \
}

static const struct iio_chan_spec nspire_adc_iio_channels[] = {
	NSPIRE_ADC_CHANNEL(0),
	NSPIRE_ADC_CHANNEL(1),
	NSPIRE_ADC_CHANNEL(2),
	NSPIRE_ADC_CHANNEL(3),
	NSPIRE_ADC_CHANNEL(4),
	NSPIRE_ADC_CHANNEL(5),
	NSPIRE_ADC_CHANNEL(6)
};

static const struct iio_info nspire_adc_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &nspire_adc_read_raw
};

static int nspire_adc_probe(struct platform_device *pdev)
{
	int ret, irq;
	struct iio_dev *iio;
	struct resource *res;
	struct nspire_adc_state *state;

	iio = iio_device_alloc(sizeof(*state));
	if (!iio)
		return -ENOMEM;

	state = iio_priv(iio);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0 || irq >= NR_IRQS) {
		dev_err(&pdev->dev, "No IRQ\n");
		ret = -ENODEV;
		goto err_free_iio;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	state->base = devm_request_and_ioremap(&pdev->dev, res);
	if (!state->base) {
		ret = -ENOMEM;
		goto err_free_iio;
	}

	ret = request_irq(irq, nspire_adc_isr, 0, "nspire_adc", state);
	if (ret) {
		dev_err(&pdev->dev, "IRQ request failed\n");
		goto err_iounmap;
	}

	iio->dev.parent = &pdev->dev;
	iio->name = dev_name(&pdev->dev);
	iio->modes = INDIO_DIRECT_MODE;
	iio->info = &nspire_adc_info;
	iio->channels = nspire_adc_iio_channels;
	iio->num_channels = ARRAY_SIZE(nspire_adc_iio_channels);

	platform_set_drvdata(pdev, iio);

	init_completion(&state->completion);

	ret = iio_device_register(iio);
	if (ret) {
		dev_err(&pdev->dev, "IIO registration failed\n");
		goto err_free_irq;
	}

	/* Enable IRQ */
	writel(~0, state->base + OFFSET_INT_MASK);
	/* Ack all channels */
	writel(~0, state->base + OFFSET_INT_RAW_STATUS);

	dev_info(&pdev->dev, "NSPIRE ADC driver registered, IRQ %d\n", irq);

	return 0;

err_free_irq:
	free_irq(irq, iio);
err_iounmap:
	iounmap(state->base);
err_free_iio:
	iio_device_free(iio);
	return ret;
}

static int nspire_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct nspire_adc_state *state = iio_priv(iio);
	int irq = platform_get_irq(pdev, 0);

	iio_device_unregister(iio);
	/* Disable IRQ */
	writel(0, state->base + OFFSET_INT_MASK);
	free_irq(irq, state);
	platform_set_drvdata(pdev, NULL);
	iounmap(state->base);
	iio_device_free(iio);

	return 0;
}

static struct platform_driver nspire_adc_driver = {
	.probe		= nspire_adc_probe,
	.remove		= nspire_adc_remove,
	.driver		= {
		.name	= "nspire-adc",
		.owner	= THIS_MODULE
	}
};

module_platform_driver(nspire_adc_driver);

MODULE_AUTHOR("Fabian Vogt <fabian@ritter-vogt.de>");
MODULE_DESCRIPTION("NSPIRE ADC driver");
MODULE_LICENSE("GPL");