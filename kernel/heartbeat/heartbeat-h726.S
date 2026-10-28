/* SPDX-License-Identifier: GPL-2.0 */
/*
 * HY300 Ultra (H726 / sun50iw15) gate-#0 heartbeat.
 *
 * Included at the very start of the 32-bit zImage decompressor
 * (arch/arm/boot/compressed/head.S), before any clock or CCU init, so it
 * runs purely on whatever state the vendor U-Boot left. If the focus MOTOR
 * ticks (a few audible clicks / a small lens move) at power-on, the vendor
 * U-Boot entered this mainline 32-bit kernel and the CPU is executing it:
 * gate #0 is passed. It is deliberately ahead of the clock tree, because
 * the CCU offsets are a hypothesis and a wrong CCU hangs the kernel soon
 * after.
 *
 * !!! DO NOT drive the cooling FAN here. !!!
 * The first version of this file toggled `fan-power-gpio` (PL7). It proved
 * gate #0 (the fan cut out = our code ran), but it also turned the fan OFF
 * while the vendor U-Boot's splash was still being projected (light engine
 * on), and after ~15 s the imager panel overheated (a dark bloom spread
 * from the centre of the projected image). Never touch the fan, and never
 * leave the device hung with the light engine on and no cooling. The motor
 * has no thermal role, so it is the safe signal.
 *
 * Target: focus-motor phase 0 = PH13 on the main PIO at 0x02000000.
 *   PH bank base = 0x02000000 + 7*0x24 = 0x020000FC.
 *   CFG1 (pins 8-15) at +0x04, pin 13 = bits [23:20]; DAT at +0x10, bit 13.
 *   output = CFG1[23:20] = 0x1 ; coil on = DAT bit 13 high.
 * (Confirmed a motor phase from the running device's GPIO map. Gate #0 is
 * already confirmed from the earlier fan run, so this is the safe form to
 * reuse, not a re-test.)
 *
 * Uses r3-r6 only and preserves r0-r2 (U-Boot passes 0 / machine id / DTB).
 */

	movw	r3, #0x00fc
	movt	r3, #0x0200		@ r3 = 0x020000FC, PH bank base

	ldr	r4, [r3, #0x04]		@ CFG1 (pins 8-15)
	bic	r4, r4, #0x00f00000	@ clear PH13 mux nibble (bits 23:20)
	orr	r4, r4, #0x00100000	@ 0x1 = output
	str	r4, [r3, #0x04]

	mov	r5, #8			@ tick count
900:	ldr	r4, [r3, #0x10]		@ DAT: coil on
	orr	r4, r4, #0x2000		@ PH13 high (bit 13)
	str	r4, [r3, #0x10]
	movw	r6, #0x0000
	movt	r6, #0x0400		@ ~0x04000000 on-delay
901:	subs	r6, r6, #1
	bne	901b
	ldr	r4, [r3, #0x10]		@ DAT: coil off
	bic	r4, r4, #0x2000
	str	r4, [r3, #0x10]
	movw	r6, #0x0000
	movt	r6, #0x0400		@ ~0x04000000 off-delay
902:	subs	r6, r6, #1
	bne	902b
	subs	r5, r5, #1
	bne	900b
	@ coil left de-energized; the fan is never touched.
