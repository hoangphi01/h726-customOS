// SPDX-License-Identifier: GPL-2.0
/*
 * Allwinner H726 (sun50iw15) CCU driver.
 *
 * sun50iw15 is the sun55i (A523) clock generation (verified from the
 * vendor kernel's clk debugfs, see ../re/ccu-sun50iw15.md), so this is
 * modelled on the mainline ccu-sun55i-a523 driver. It implements the
 * core bootable subset first (PLLs plus the UART / MMC / TWI / PWM / DMA
 * gates); the tree grows from here.
 *
 * Clock rates and parents are hardware-verified; the register offsets
 * are the A523 generation's as a working hypothesis and are confirmed
 * against the hardware once a serial console allows boot testing.
 *
 * Based on ccu-sun55i-a523.c (Copyright (C) 2023-2024 Arm Ltd.),
 * itself based on the D1 CCU driver (Samuel Holland, Allwinner).
 */

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "../clk.h"
#include "ccu_common.h"
#include "ccu_reset.h"
#include "ccu_div.h"
#include "ccu_gate.h"
#include "ccu_mp.h"
#include "ccu_mult.h"
#include "ccu_nkmp.h"

#include "sun50iw15-ccu.h"
#include "sun50iw15-ccu-reset.h"

/* The 24 MHz oscillator (dcxo24M), root of most of the tree. */
static const struct clk_parent_data osc24M[] = {
	{ .fw_name = "hosc" }
};

/*
 * PLLs: input * N / M / P. Modelled as NKMP with no K.
 * Offsets are the A523 generation's (TODO: confirm on hardware).
 */
#define SUN50IW15_PLL_CPU_REG		0x000
static struct ccu_nkmp pll_cpu_clk = {
	.enable		= BIT(27),
	.lock		= BIT(28),
	.n		= _SUNXI_CCU_MULT_MIN(8, 8, 11),
	.m		= _SUNXI_CCU_DIV(1, 1),
	.p		= _SUNXI_CCU_DIV(16, 2),
	.common		= {
		.reg		= 0x000,
		.hw.init	= CLK_HW_INIT_PARENTS_DATA("pll-cpu", osc24M,
						&ccu_nkmp_ops,
						CLK_SET_RATE_UNGATE),
	},
};

#define SUN50IW15_PLL_DDR0_REG		0x010
static struct ccu_nkmp pll_ddr0_clk = {
	.enable		= BIT(27),
	.lock		= BIT(28),
	.n		= _SUNXI_CCU_MULT_MIN(8, 8, 11),
	.m		= _SUNXI_CCU_DIV(1, 1),
	.p		= _SUNXI_CCU_DIV(0, 1),
	.common		= {
		.reg		= 0x010,
		.hw.init	= CLK_HW_INIT_PARENTS_DATA("pll-ddr0", osc24M,
						&ccu_nkmp_ops,
						CLK_SET_RATE_UNGATE |
						CLK_IS_CRITICAL),
	},
};

#define SUN50IW15_PLL_PERI0_REG		0x020
static struct ccu_nkmp pll_peri0_clk = {
	.enable		= BIT(27),
	.lock		= BIT(28),
	.n		= _SUNXI_CCU_MULT_MIN(8, 8, 11),
	.m		= _SUNXI_CCU_DIV(1, 1),
	.p		= _SUNXI_CCU_DIV(0, 1),
	.common		= {
		.reg		= 0x020,
		.hw.init	= CLK_HW_INIT_PARENTS_DATA("pll-peri0", osc24M,
						&ccu_nkmp_ops,
						CLK_SET_RATE_UNGATE |
						CLK_IS_CRITICAL),
	},
};

/*
 * Bus gates. Parented to the 24 MHz oscillator as a placeholder; the
 * real parents (apb-uart, ahb, smhc module clocks) land with the mux /
 * divider clocks in a later pass. Offsets are the A523 generation's.
 */
static SUNXI_CCU_GATE_DATA(bus_uart0_clk, "bus-uart0", osc24M, 0x90c, BIT(0), 0);
static SUNXI_CCU_GATE_DATA(bus_uart1_clk, "bus-uart1", osc24M, 0x90c, BIT(1), 0);
static SUNXI_CCU_GATE_DATA(bus_uart2_clk, "bus-uart2", osc24M, 0x90c, BIT(2), 0);
static SUNXI_CCU_GATE_DATA(bus_uart3_clk, "bus-uart3", osc24M, 0x90c, BIT(3), 0);
static SUNXI_CCU_GATE_DATA(bus_twi0_clk,  "bus-twi0",  osc24M, 0x91c, BIT(0), 0);
static SUNXI_CCU_GATE_DATA(bus_twi1_clk,  "bus-twi1",  osc24M, 0x91c, BIT(1), 0);
static SUNXI_CCU_GATE_DATA(smhc0_clk,     "smhc0",     osc24M, 0x830, BIT(31), 0);
static SUNXI_CCU_GATE_DATA(smhc1_clk,     "smhc1",     osc24M, 0x834, BIT(31), 0);
static SUNXI_CCU_GATE_DATA(smhc2_clk,     "smhc2",     osc24M, 0x838, BIT(31), 0);
static SUNXI_CCU_GATE_DATA(bus_smhc0_clk, "bus-smhc0", osc24M, 0x84c, BIT(0), 0);
static SUNXI_CCU_GATE_DATA(bus_smhc1_clk, "bus-smhc1", osc24M, 0x84c, BIT(1), 0);
static SUNXI_CCU_GATE_DATA(bus_smhc2_clk, "bus-smhc2", osc24M, 0x84c, BIT(2), 0);
static SUNXI_CCU_GATE_DATA(bus_pwm_clk,   "bus-pwm",   osc24M, 0x7ac, BIT(0), 0);
static SUNXI_CCU_GATE_DATA(bus_dma_clk,   "bus-dma",   osc24M, 0x70c, BIT(0), 0);
static SUNXI_CCU_GATE_DATA(bus_gpu_clk,   "bus-gpu",   osc24M, 0x67c, BIT(0), 0);
static SUNXI_CCU_GATE_DATA(gpu_core_clk,  "gpu-core",  osc24M, 0x670, BIT(31), 0);
static SUNXI_CCU_GATE_DATA(bus_ve_clk,    "bus-ve",    osc24M, 0x69c, BIT(0), 0);
static SUNXI_CCU_GATE_DATA(ve_clk,        "ve",        osc24M, 0x690, BIT(31), 0);

static struct ccu_common *sun50iw15_ccu_clks[] = {
	&pll_cpu_clk.common,
	&pll_ddr0_clk.common,
	&pll_peri0_clk.common,
	&bus_uart0_clk.common,
	&bus_uart1_clk.common,
	&bus_uart2_clk.common,
	&bus_uart3_clk.common,
	&bus_twi0_clk.common,
	&bus_twi1_clk.common,
	&smhc0_clk.common,
	&smhc1_clk.common,
	&smhc2_clk.common,
	&bus_smhc0_clk.common,
	&bus_smhc1_clk.common,
	&bus_smhc2_clk.common,
	&bus_pwm_clk.common,
	&bus_dma_clk.common,
	&bus_gpu_clk.common,
	&gpu_core_clk.common,
	&bus_ve_clk.common,
	&ve_clk.common,
};

static struct clk_hw_onecell_data sun50iw15_hw_clks = {
	.hws	= {
		[CLK_PLL_CPU]	= &pll_cpu_clk.common.hw,
		[CLK_PLL_DDR0]	= &pll_ddr0_clk.common.hw,
		[CLK_PLL_PERI0]	= &pll_peri0_clk.common.hw,
		[CLK_BUS_UART0]	= &bus_uart0_clk.common.hw,
		[CLK_BUS_UART1]	= &bus_uart1_clk.common.hw,
		[CLK_BUS_UART2]	= &bus_uart2_clk.common.hw,
		[CLK_BUS_UART3]	= &bus_uart3_clk.common.hw,
		[CLK_BUS_TWI0]	= &bus_twi0_clk.common.hw,
		[CLK_BUS_TWI1]	= &bus_twi1_clk.common.hw,
		[CLK_SMHC0]	= &smhc0_clk.common.hw,
		[CLK_SMHC1]	= &smhc1_clk.common.hw,
		[CLK_SMHC2]	= &smhc2_clk.common.hw,
		[CLK_BUS_SMHC0]	= &bus_smhc0_clk.common.hw,
		[CLK_BUS_SMHC1]	= &bus_smhc1_clk.common.hw,
		[CLK_BUS_SMHC2]	= &bus_smhc2_clk.common.hw,
		[CLK_BUS_PWM]	= &bus_pwm_clk.common.hw,
		[CLK_BUS_DMA]	= &bus_dma_clk.common.hw,
		[CLK_BUS_GPU]	= &bus_gpu_clk.common.hw,
		[CLK_GPU_CORE]	= &gpu_core_clk.common.hw,
		[CLK_BUS_VE]	= &bus_ve_clk.common.hw,
		[CLK_VE]	= &ve_clk.common.hw,
	},
	.num	= CLK_NUMBER,
};

static struct ccu_reset_map sun50iw15_ccu_resets[] = {
	[RST_BUS_UART0]	= { 0x90c, BIT(16) },
	[RST_BUS_UART1]	= { 0x90c, BIT(17) },
	[RST_BUS_UART2]	= { 0x90c, BIT(18) },
	[RST_BUS_UART3]	= { 0x90c, BIT(19) },
	[RST_BUS_TWI0]	= { 0x91c, BIT(16) },
	[RST_BUS_TWI1]	= { 0x91c, BIT(17) },
	[RST_BUS_SMHC0]	= { 0x84c, BIT(16) },
	[RST_BUS_SMHC1]	= { 0x84c, BIT(17) },
	[RST_BUS_SMHC2]	= { 0x84c, BIT(18) },
	[RST_BUS_PWM]	= { 0x7ac, BIT(16) },
	[RST_BUS_DMA]	= { 0x70c, BIT(16) },
	[RST_BUS_GPU]	= { 0x67c, BIT(16) },
	[RST_BUS_VE]	= { 0x69c, BIT(16) },
};

static const struct sunxi_ccu_desc sun50iw15_ccu_desc = {
	.ccu_clks	= sun50iw15_ccu_clks,
	.num_ccu_clks	= ARRAY_SIZE(sun50iw15_ccu_clks),

	.hw_clks	= &sun50iw15_hw_clks,

	.resets		= sun50iw15_ccu_resets,
	.num_resets	= ARRAY_SIZE(sun50iw15_ccu_resets),
};

static int sun50iw15_ccu_probe(struct platform_device *pdev)
{
	void __iomem *reg;

	reg = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	return devm_sunxi_ccu_probe(&pdev->dev, reg, &sun50iw15_ccu_desc);
}

static const struct of_device_id sun50iw15_ccu_ids[] = {
	{ .compatible = "allwinner,sun50iw15-ccu" },
	{ }
};

static struct platform_driver sun50iw15_ccu_driver = {
	.probe	= sun50iw15_ccu_probe,
	.driver	= {
		.name			= "sun50iw15-ccu",
		.suppress_bind_attrs	= true,
		.of_match_table		= sun50iw15_ccu_ids,
	},
};
module_platform_driver(sun50iw15_ccu_driver);

MODULE_DESCRIPTION("Allwinner H726 (sun50iw15) CCU driver");
MODULE_LICENSE("GPL");
