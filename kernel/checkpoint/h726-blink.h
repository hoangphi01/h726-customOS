/* SPDX-License-Identifier: GPL-2.0 */
/*
 * HY300 Ultra (H726 / sun50iw15) assembly checkpoint-beacon macro.
 *
 * No-UART bring-up tool. `H726_BLINK n, rb, rt, rc, rd` ticks the focus
 * MOTOR phase PH13 `n` times, so progress through the early boot can be
 * seen/heard without a serial console. The macro only uses the four
 * registers it is handed, so it can be dropped in at sites with very
 * different register discipline (the decompressor, where most registers
 * are free, and kernel-proper head.S, where only r0/r3/r6/r7 are).
 *
 * It writes only movw/movt immediates (no literal pool, no memory loads)
 * and PC-relative branches, so it is position-independent and safe with
 * the MMU either off (decompressor / head.S before __turn_mmu_on) or on.
 *
 * !!! SAFETY: this only ever drives the focus MOTOR (PH13). Never the
 * cooling fan (PL7 / PWM0). Turning the fan off while the light engine is
 * on overheats the imager panel in ~15 s (see ../bringup-strategy.md). The
 * motor has no thermal role, so it is the safe signal.
 *
 * Target: focus-motor phase 0 = PH13 on the main PIO at 0x02000000.
 *   PH bank base = 0x02000000 + 7*0x24 = 0x020000FC.
 *   CFG1 (pins 8-15) at +0x04, pin 13 mux = bits [23:20]; DAT at +0x10 bit 13.
 *
 *   \rb  scratch: PH bank base address
 *   \rt  scratch: value temp
 *   \rc  scratch: tick counter
 *   \rd  scratch: delay counter
 * All four are clobbered; nothing else is touched.
 */

	.macro	H726_BLINK count, rb, rt, rc, rd
	movw	\rb, #0x00fc
	movt	\rb, #0x0200			@ \rb = 0x020000FC, PH bank base
	ldr	\rt, [\rb, #0x04]		@ CFG1 (pins 8-15)
	bic	\rt, \rt, #0x00f00000		@ clear PH13 mux nibble [23:20]
	orr	\rt, \rt, #0x00100000		@ 0x1 = output
	str	\rt, [\rb, #0x04]
	mov	\rc, #\count
8881:	ldr	\rt, [\rb, #0x10]		@ DAT: coil on
	orr	\rt, \rt, #0x2000		@ PH13 high (bit 13)
	str	\rt, [\rb, #0x10]
	movw	\rd, #0x0000
	movt	\rd, #0x0300			@ ~on delay
8882:	subs	\rd, \rd, #1
	bne	8882b
	ldr	\rt, [\rb, #0x10]		@ DAT: coil off
	bic	\rt, \rt, #0x2000
	str	\rt, [\rb, #0x10]
	movw	\rd, #0x0000
	movt	\rd, #0x0600			@ ~off delay (longer gap between ticks)
8883:	subs	\rd, \rd, #1
	bne	8883b
	subs	\rc, \rc, #1
	bne	8881b
	@ coil left de-energized; the fan is never touched.
	.endm
