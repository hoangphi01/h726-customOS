// SPDX-License-Identifier: GPL-2.0
/*
 * HY300 Ultra (H726 / sun50iw15) C-side boot checkpoints for no-UART
 * bring-up. Blinks the focus MOTOR (PH13) from C so progress past the
 * assembly head can be seen without a serial console:
 *
 *   - early_initcall  -> 3 ticks: the kernel reached the init thread
 *                        (setup_arch + mm + sched + timers are all up).
 *   - late_initcall   -> a steady ~1 Hz heartbeat kthread: the kernel is
 *                        fully alive and the scheduler/timers are ticking.
 *
 * Counts pair with the assembly beacons (1 tick = decompressor/gate #0,
 * 2 ticks = entered vmlinux pre-MMU); the highest thing observed is how
 * far the boot got. A steady heartbeat is the unambiguous "it booted"
 * signal when there is no console to print one.
 *
 * !!! SAFETY: only ever touches the focus MOTOR (PH13). Never the cooling
 * fan (PL7 / PWM0): turning the fan off with the light engine on overheats
 * the panel. The motor has no thermal role. We poke the PIO directly (not
 * through the gpio/pinctrl API) on purpose, so the beacon works even if no
 * driver has probed yet.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/err.h>

#define PIO_BASE	0x02000000	/* main pin controller */
#define PIO_SIZE	0x400
#define PH_BANK		0xFC		/* PH = bank 7, 7 * 0x24 */
#define PH_CFG1		(PH_BANK + 0x04) /* pins 8-15 mux */
#define PH_DAT		(PH_BANK + 0x10)
#define PH13_MUX_MASK	0x00F00000	/* pin 13 nibble [23:20] */
#define PH13_MUX_OUT	0x00100000	/* 0x1 = output */
#define PH13_BIT	(1u << 13)

static void __iomem *h726_pio;

static int h726_map(void)
{
	if (!h726_pio)
		h726_pio = ioremap(PIO_BASE, PIO_SIZE);
	return h726_pio ? 0 : -ENOMEM;
}

static void h726_ph13_output(void)
{
	u32 v = readl(h726_pio + PH_CFG1);

	v &= ~PH13_MUX_MASK;
	v |= PH13_MUX_OUT;
	writel(v, h726_pio + PH_CFG1);
}

static void h726_tick(unsigned int on_ms, unsigned int off_ms)
{
	u32 v;

	v = readl(h726_pio + PH_DAT);
	writel(v | PH13_BIT, h726_pio + PH_DAT);	/* coil on */
	mdelay(on_ms);
	v = readl(h726_pio + PH_DAT);
	writel(v & ~PH13_BIT, h726_pio + PH_DAT);	/* coil off */
	if (off_ms)
		mdelay(off_ms);
}

static void h726_blink(unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		h726_tick(80, 150);
}

/* CP: reached the init thread. 3 ticks. */
static int __init h726_cp_init(void)
{
	if (h726_map())
		return 0;
	h726_ph13_output();
	h726_blink(3);
	return 0;
}
early_initcall(h726_cp_init);

/* Steady heartbeat: kernel fully alive (scheduler + timekeeping running). */
static int h726_heartbeat_fn(void *unused)
{
	while (!kthread_should_stop()) {
		h726_tick(60, 0);
		msleep(940);
	}
	return 0;
}

static int __init h726_heartbeat_start(void)
{
	if (h726_map())
		return 0;
	h726_ph13_output();
	kthread_run(h726_heartbeat_fn, NULL, "h726-heartbeat");
	return 0;
}
late_initcall(h726_heartbeat_start);
