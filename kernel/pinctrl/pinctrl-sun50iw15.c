// SPDX-License-Identifier: GPL-2.0
/*
 * Allwinner H726 (sun50iw15) pin controller driver.
 *
 * Modelled on the mainline H616 pinctrl driver. First pass: the pins the
 * board device tree uses (uart0 console, eMMC on PC, the sensor I2C). The
 * pin-to-function mux values are the sun50iw15 layout and are confirmed
 * against hardware once a serial console allows boot testing; the bank
 * structure and the driver scaffolding are what matter here.
 *
 * Based on pinctrl-sun50i-h616.c (Copyright (C) 2020 Arm Ltd.),
 * itself based on the H6 pinctrl driver (Icenowy Zheng).
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-sunxi.h"

static const struct sunxi_desc_pin sun50iw15_pins[] = {
	/* UART0 debug console */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(H, 0),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "uart0")),	/* TX */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(H, 1),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "uart0")),	/* RX */

	/* eMMC (mmc2) on port C */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(C, 1),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "mmc2")),
	SUNXI_PIN(SUNXI_PINCTRL_PIN(C, 4),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "mmc2")),
	SUNXI_PIN(SUNXI_PINCTRL_PIN(C, 5),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "mmc2")),
	SUNXI_PIN(SUNXI_PINCTRL_PIN(C, 6),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "mmc2")),
	SUNXI_PIN(SUNXI_PINCTRL_PIN(C, 7),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "mmc2")),
	SUNXI_PIN(SUNXI_PINCTRL_PIN(C, 8),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "mmc2")),
	SUNXI_PIN(SUNXI_PINCTRL_PIN(C, 9),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "mmc2")),
	SUNXI_PIN(SUNXI_PINCTRL_PIN(C, 10),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "mmc2")),

	/* Sensor I2C on port B */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(B, 18),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x4, "i2c1")),		/* SCK */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(B, 19),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x4, "i2c1")),		/* SDA */
};

static const unsigned int sun50iw15_irq_bank_map[] = { 0, 1, 2, 3, 4, 5 };

static const struct sunxi_pinctrl_desc sun50iw15_pinctrl_data = {
	.pins = sun50iw15_pins,
	.npins = ARRAY_SIZE(sun50iw15_pins),
	.irq_banks = ARRAY_SIZE(sun50iw15_irq_bank_map),
	.irq_bank_map = sun50iw15_irq_bank_map,
	.irq_read_needs_mux = true,
	.io_bias_cfg_variant = BIAS_VOLTAGE_PIO_POW_MODE_CTL,
};

static int sun50iw15_pinctrl_probe(struct platform_device *pdev)
{
	return sunxi_pinctrl_init(pdev, &sun50iw15_pinctrl_data);
}

static const struct of_device_id sun50iw15_pinctrl_match[] = {
	{ .compatible = "allwinner,sun50iw15-pinctrl", },
	{}
};

static struct platform_driver sun50iw15_pinctrl_driver = {
	.probe	= sun50iw15_pinctrl_probe,
	.driver	= {
		.name		= "sun50iw15-pinctrl",
		.of_match_table	= sun50iw15_pinctrl_match,
	},
};
builtin_platform_driver(sun50iw15_pinctrl_driver);
