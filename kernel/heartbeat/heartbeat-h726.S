/* SPDX-License-Identifier: GPL-2.0 */
/*
 * HY300 Ultra (H726 / sun50iw15) gate-#0 heartbeat.
 *
 * Included at the very start of the 32-bit zImage decompressor
 * (arch/arm/boot/compressed/head.S), before any clock or CCU init, so it
 * runs purely on whatever state the vendor U-Boot left. If the red LED
 * blinks five times, the vendor U-Boot entered this mainline 32-bit
 * kernel and the CPU is executing it: gate #0 is passed. This is deliberately
 * ahead of the clock tree, because the CCU offsets are still a hypothesis
 * and a wrong CCU would hang the kernel long before any later heartbeat.
 *
 * The MMU is off here, so we poke the physical GPIO registers directly, no
 * clock needed. Target: the red power LED on PL0.
 *   R_PIO controller base = 0x07022000 (the PL power-domain bank).
 *   PL bank: CFG0 at +0x00 (4 bits per pin), DAT at +0x10.
 *   PL0 as output  = CFG0[3:0] = 0x1.
 *   LED on         = DAT bit 0 (active high, as on the H713 sibling).
 *
 * Best guess from the HY300 family (H713): the pin may differ on this unit.
 * To retarget, change the base / bank / bit below (see ../heartbeat.md).
 * Uses r3-r6 only and preserves r0-r2 (U-Boot passes 0 / machine id / DTB).
 * Recoverable: a wrong guess just means the LED does not blink; reflash the
 * stock image over FEL with PhoenixSuit.
 */

	movw	r3, #0x2000
	movt	r3, #0x0702		@ r3 = 0x07022000, R_PIO (PL) base

	ldr	r4, [r3, #0x00]		@ CFG0
	bic	r4, r4, #0x0000000f	@ clear PL0 mux nibble
	orr	r4, r4, #0x00000001	@ 0x1 = output
	str	r4, [r3, #0x00]

	mov	r5, #5			@ blink count
900:	ldr	r4, [r3, #0x10]		@ DAT: LED on
	orr	r4, r4, #1
	str	r4, [r3, #0x10]
	movw	r6, #0x0000
	movt	r6, #0x0030		@ ~0x300000 on-delay
901:	subs	r6, r6, #1
	bne	901b
	ldr	r4, [r3, #0x10]		@ DAT: LED off
	bic	r4, r4, #1
	str	r4, [r3, #0x10]
	movw	r6, #0x0000
	movt	r6, #0x0030		@ ~0x300000 off-delay
902:	subs	r6, r6, #1
	bne	902b
	subs	r5, r5, #1
	bne	900b
