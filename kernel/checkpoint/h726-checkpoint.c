// SPDX-License-Identifier: GPL-2.0
/*
 * HY300 Ultra (H726 / sun50iw15) C-side boot checkpoints for no-UART
 * bring-up. The signal is the COOLING FAN: brief OFF->ON pulses that the
 * operator hears as a stutter in the (loud, constant) fan. It is the one
 * channel proven observable on this unit - the motor is inaudible and does
 * not move the manual-focus image, the status LED is not a SoC GPIO, and
 * reading the vendor /sys/class/prj backlight reboots the device.
 *
 *   - early_initcall  -> 3 fan stutters, then returns: the kernel reached the
 *                        init thread (setup_arch + mm + sched + timers are up).
 *   - late_initcall   -> stutter forever (blocking): the kernel is fully alive.
 *                        It deliberately never returns, so boot never reaches
 *                        the (rootfs-less) panic; the fan just keeps stuttering.
 *
 * Read it as: a few stutters then steady fan = reached init but stopped;
 * endless stuttering = booted. The early assembly beacons CP1/CP2 add 1 and 2
 * stutters before this.
 *
 * SAFETY: a pulse turns the fan off only briefly, with interrupts disabled so
 * nothing preempts the off window, and ALWAYS restores it ON. The fan's
 * resting state is ON. See h726-blink.h for the thermal rationale.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/err.h>

#define RPIO_BASE	0x07022000	/* R_PIO; PL = bank 0 at this base */
#define RPIO_SIZE	0x400
#define PL_CFG0		0x00		/* pins 0-7 mux */
#define PL_DAT		0x10
#define PL7_MUX_MASK	0xF0000000u	/* pin 7 nibble [31:28] */
#define PL7_MUX_OUT	0x10000000u	/* 0x1 = output */
#define PL7_BIT		(1u << 7)	/* fan enable: high = ON */

#define OFF_MS		1000		/* fan off ~1 s (audibly stops) */
#define ON_MS		2000		/* fan on ~2 s (spins back up) */

static void __iomem *h726_rpio;

static int h726_map(void)
{
	if (!h726_rpio)
		h726_rpio = ioremap(RPIO_BASE, RPIO_SIZE);
	return h726_rpio ? 0 : -ENOMEM;
}

static void h726_fan(bool on)
{
	u32 v = readl(h726_rpio + PL_DAT);

	if (on)
		v |= PL7_BIT;
	else
		v &= ~PL7_BIT;
	writel(v, h726_rpio + PL_DAT);
}

static void h726_fan_output(void)
{
	u32 v = readl(h726_rpio + PL_CFG0);

	v &= ~PL7_MUX_MASK;
	v |= PL7_MUX_OUT;
	writel(v, h726_rpio + PL_CFG0);
	h726_fan(true);			/* start from the ON state */
}

/* One stutter. IRQs off across the off window so nothing preempts it. */
static void h726_stutter(void)
{
	unsigned long flags;

	local_irq_save(flags);
	h726_fan(false);
	mdelay(OFF_MS);
	h726_fan(true);
	local_irq_restore(flags);
	mdelay(ON_MS);			/* fan running in the gap */
}

/*
 * Reached the init thread (setup_arch + mm + sched + timers are up). This is
 * already the big milestone - it means the memreserve/DT fix let the kernel
 * through setup_arch. Stutter the fan FOREVER so it cannot be missed: a short
 * three-stutter "hello", then a slow endless stutter. It never returns (there
 * is no rootfs to go to), so the fan just keeps stuttering = the kernel is
 * alive here. If instead you hear only the early CP1/CP2 taps then silence,
 * it reached vmlinux but hung in setup_arch.
 */
static int __init h726_cp_init(void)
{
	int i;

	if (h726_map())
		return 0;
	h726_fan_output();
	for (i = 0; i < 3; i++)		/* "hello": three quick-ish stutters */
		h726_stutter();
	for (;;)			/* then alive-forever */
		h726_stutter();
	return 0;
}
early_initcall(h726_cp_init);
