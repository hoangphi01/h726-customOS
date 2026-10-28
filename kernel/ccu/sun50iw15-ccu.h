/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Device-tree binding constants for the Allwinner H726 (sun50iw15) CCU.
 * Clock and reset indices for the main clock controller at 0x2002000.
 *
 * This is our own numbering (the device tree references these), covering
 * the core bootable subset first; it grows as the driver grows. The tree
 * these name is documented in ../re/ccu-sun50iw15.md (read live from the
 * vendor kernel's clk debugfs).
 *
 * Install as include/dt-bindings/clock/sun50iw15-ccu.h and
 * include/dt-bindings/reset/sun50iw15-ccu.h in the kernel tree.
 */

#ifndef _DT_BINDINGS_CLOCK_SUN50IW15_CCU_H_
#define _DT_BINDINGS_CLOCK_SUN50IW15_CCU_H_

/* PLLs */
#define CLK_PLL_CPU		0
#define CLK_PLL_DDR0		1
#define CLK_PLL_PERI0		2
#define CLK_PLL_PERI0_480M	3
#define CLK_PLL_PERI0_800M	4
#define CLK_PLL_PERI0_400M	5
#define CLK_PLL_PERI0_200M	6
#define CLK_PLL_PERI1		7
#define CLK_PLL_GPU		8
#define CLK_PLL_VIDEO2		9
#define CLK_PLL_VE		10

/* CPU / bus */
#define CLK_CPU			16
#define CLK_CPU_AXI		17
#define CLK_CPU_APB		18
#define CLK_AHB0		19
#define CLK_APB1		20
#define CLK_APB_UART		21
#define CLK_MBUS		22

/* Peripheral gates */
#define CLK_BUS_UART0		32
#define CLK_BUS_UART1		33
#define CLK_BUS_UART2		34
#define CLK_BUS_UART3		35
#define CLK_BUS_TWI0		40
#define CLK_BUS_TWI1		41
#define CLK_SMHC0		48
#define CLK_SMHC1		49
#define CLK_SMHC2		50
#define CLK_BUS_SMHC0		51
#define CLK_BUS_SMHC1		52
#define CLK_BUS_SMHC2		53
#define CLK_BUS_PWM		56
#define CLK_BUS_DMA		57
#define CLK_BUS_GPU		58
#define CLK_GPU_CORE		59
#define CLK_BUS_VE		60
#define CLK_VE			61

#define CLK_NUMBER		64

#endif /* _DT_BINDINGS_CLOCK_SUN50IW15_CCU_H_ */
