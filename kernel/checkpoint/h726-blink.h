/* SPDX-License-Identifier: GPL-2.0 */
/*
 * HY300 Ultra (H726 / sun50iw15) assembly checkpoint-beacon macro (FAN).
 *
 * No-UART bring-up tool. `H726_BLINK n, rb, rt, rc, rd` pulses the cooling
 * fan OFF->ON `n` times. The fan is the loudest thing in the room and runs
 * the whole time, so a brief stutter in it is the one signal that can be
 * perceived over itself (the motor is inaudible and, on this unit, does not
 * move the manual-focus image; the status LED is not a SoC GPIO). It is the
 * signal that already confirmed gate #0.
 *
 * SAFETY: each pulse turns the fan off only briefly and ALWAYS turns it back
 * ON before the macro returns, and the whole pulse is a tight inline loop
 * (no calls, no allocation), so the kernel cannot wander off with the fan
 * off. The thermal incident happened the other way: a beacon turned the fan
 * off and then the kernel hung with it off. Here the fan's resting state is
 * always ON. The operator still watches for the fan going *fully* silent
 * (a hang caught mid-pulse) and cuts power; the ~15 s thermal margin covers it.
 *
 * Fan power enable = PL7 on the R_PIO at 0x07022000.
 *   PL = R_PIO bank 0, base 0x07022000. CFG0 (pins 0-7) at +0x00, pin 7 mux
 *   = bits [31:28]; DAT at +0x10, bit 7. PL7 high = fan ON, low = fan OFF.
 *
 * Uses only movw/movt immediates and PC-relative branches (position
 * independent, MMU on or off). Clobbers only the four registers handed in.
 */

	.macro	H726_BLINK count, rb, rt, rc, rd
	movw	\rb, #0x2000
	movt	\rb, #0x0702			@ \rb = 0x07022000, R_PIO / PL bank
	ldr	\rt, [\rb, #0x00]		@ CFG0 (pins 0-7)
	bic	\rt, \rt, #0xf0000000		@ clear PL7 mux nibble [31:28]
	orr	\rt, \rt, #0x10000000		@ 0x1 = output
	str	\rt, [\rb, #0x00]
	ldr	\rt, [\rb, #0x10]		@ make sure the fan starts ON
	orr	\rt, \rt, #0x80			@ PL7 high
	str	\rt, [\rb, #0x10]
	mov	\rc, #\count
8881:	ldr	\rt, [\rb, #0x10]		@ fan OFF
	bic	\rt, \rt, #0x80
	str	\rt, [\rb, #0x10]
	movw	\rd, #0x0000
	movt	\rd, #0x2000			@ ~1 s off (fan audibly stops)
8882:	subs	\rd, \rd, #1
	bne	8882b
	ldr	\rt, [\rb, #0x10]		@ fan ON
	orr	\rt, \rt, #0x80
	str	\rt, [\rb, #0x10]
	movw	\rd, #0x0000
	movt	\rd, #0x4000			@ ~2 s on (fan spins back up)
8883:	subs	\rd, \rd, #1
	bne	8883b
	subs	\rc, \rc, #1
	bne	8881b
	@ fan left ON.
	.endm

/*
 * Diagnostic: one short fan blip to mark "the kernel reached here", then SPIN
 * FOREVER with the fan left ON. It never decompresses the kernel. This answers
 * one question: is the ~1-2 s "one stutter then standby" an EXTERNAL reset
 * (watchdog / MCU) or our kernel progressing and crashing?
 *   - blip, then the device STAYS ON (splash frozen, blue LED, fan running) for
 *     a long time = the SoC spins happily, NO external reset -> the earlier
 *     resets were our kernel crashing downstream once it decompressed and ran.
 *   - blip, then RED standby at ~2 s = an external watchdog/MCU cuts power at
 *     ~2 s regardless of what the SoC does.
 * The fan is only ever off for the brief blip and is left ON for the spin, so
 * it is thermally safe even though the spin is endless.
 */
/*
 * BEEP test: drive the speaker power-amplifier ENABLE pin (PL2 on the R_PIO,
 * the codec's pa-pin, active high) as a square wave, so the amp's on/off
 * switching transients make an audible buzz/beep - no audio codec, DAC or
 * clock needed, just a GPIO toggle. Buzz ~1 s, then SPIN FOREVER leaving the
 * fan untouched (U-Boot keeps it running, so thermally safe). Dual purpose:
 *   - a clear beep = far better than the fan as a bring-up signal; and
 *   - beep then device STAYS ON = SoC spins, no external reset; beep then RED
 *     standby at ~2 s = an external watchdog/MCU cut.
 * PL2 = R_PIO 0x07022000, PL bank: CFG0 at +0x00 bits[11:8]=output, DAT +0x10 bit 2.
 */
	.macro	H726_BEEP rb, rt, rc, rd
	movw	\rb, #0x2000
	movt	\rb, #0x0702			@ R_PIO / PL bank
	ldr	\rt, [\rb, #0x00]
	bic	\rt, \rt, #0x00000f00
	orr	\rt, \rt, #0x00000100		@ PL2 output
	str	\rt, [\rb, #0x00]
	movw	\rc, #0x8000
	movt	\rc, #0x0000			@ ~32768 toggles
8991:	ldr	\rt, [\rb, #0x10]		@ toggle PL2 (square wave)
	eor	\rt, \rt, #0x04
	str	\rt, [\rb, #0x10]
	movw	\rd, #0x0000
	movt	\rd, #0x0008			@ half-period delay (sets pitch)
8992:	subs	\rd, \rd, #1
	bne	8992b
	subs	\rc, \rc, #1
	bne	8991b
	ldr	\rt, [\rb, #0x10]		@ PL2 low (amp off)
	bic	\rt, \rt, #0x04
	str	\rt, [\rb, #0x10]
8993:	b	8993b				@ spin forever, fan untouched
	.endm

	.macro	H726_SPINTEST rb, rt, rc, rd
	movw	\rb, #0x2000
	movt	\rb, #0x0702			@ R_PIO / PL bank
	ldr	\rt, [\rb, #0x00]
	bic	\rt, \rt, #0xf0000000
	orr	\rt, \rt, #0x10000000		@ PL7 output
	str	\rt, [\rb, #0x00]
	ldr	\rt, [\rb, #0x10]		@ fan OFF (brief blip)
	bic	\rt, \rt, #0x80
	str	\rt, [\rb, #0x10]
	movw	\rd, #0x0000
	movt	\rd, #0x2000			@ short, safe (the known "tach")
7772:	subs	\rd, \rd, #1
	bne	7772b
	ldr	\rt, [\rb, #0x10]		@ fan ON (and left on)
	orr	\rt, \rt, #0x80
	str	\rt, [\rb, #0x10]
7773:	b	7773b				@ spin forever, fan running
	.endm
