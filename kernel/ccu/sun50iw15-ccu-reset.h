/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Reset indices for the Allwinner H726 (sun50iw15) CCU.
 * Install as include/dt-bindings/reset/sun50iw15-ccu.h.
 */

#ifndef _DT_BINDINGS_RESET_SUN50IW15_CCU_H_
#define _DT_BINDINGS_RESET_SUN50IW15_CCU_H_

#define RST_BUS_UART0	0
#define RST_BUS_UART1	1
#define RST_BUS_UART2	2
#define RST_BUS_UART3	3
#define RST_BUS_TWI0	4
#define RST_BUS_TWI1	5
#define RST_BUS_SMHC0	8
#define RST_BUS_SMHC1	9
#define RST_BUS_SMHC2	10
#define RST_BUS_PWM	12
#define RST_BUS_DMA	13
#define RST_BUS_GPU	14
#define RST_BUS_VE	15

#define RST_NUMBER	16

#endif /* _DT_BINDINGS_RESET_SUN50IW15_CCU_H_ */
