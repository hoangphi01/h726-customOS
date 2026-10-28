// SPDX-License-Identifier: GPL-2.0
/*
 * HY300 Ultra (H726 / sun50iw15) C-side boot checkpoints for no-UART
 * bring-up. The cooling fan runs loud the whole time, so an audible motor
 * tick is useless; instead we step the focus MOTOR through its four phases
 * to physically move the lens, which makes the PROJECTED IMAGE visibly
 * defocus then refocus - a "breath" that is obvious even over the fan:
 *
 *   - early_initcall  -> 2 breaths: the kernel reached the init thread
 *                        (setup_arch + mm + sched + timers are all up).
 *   - late_initcall   -> a steady breath every ~1.5 s, forever: the kernel
 *                        is fully alive and the scheduler/timers are ticking.
 *
 * So watch the projected image: a couple of focus breaths then nothing means
 * it reached init but hung at late init; a never-ending slow focus breathing
 * means it booted. (The earlier assembly beacons CP1/CP2 only tap one coil -
 * a tiny jerk, hard to see over the fan - so the C breaths are the signal.)
 *
 * !!! SAFETY: only ever drives the focus-MOTOR phases (PH6/PH7/PH13/PH18).
 * Never the cooling fan (PL7 / PWM0): turning the fan off with the light
 * engine on overheats the panel. The motor has no thermal role. We poke the
 * PIO directly (not via the gpio/pinctrl API) so the beacon works before any
 * driver has probed, and read-modify-write the DAT register so only the four
 * motor bits change.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/err.h>

#define PIO_BASE	0x02000000	/* main pin controller */
#define PIO_SIZE	0x400
#define PH_BANK		0xFC		/* PH = bank 7, 7 * 0x24 */
#define PH_CFG0		(PH_BANK + 0x00) /* pins 0-7   */
#define PH_CFG1		(PH_BANK + 0x04) /* pins 8-15  */
#define PH_CFG2		(PH_BANK + 0x08) /* pins 16-23 */
#define PH_DAT		(PH_BANK + 0x10)

/* Focus-motor unipolar phases on the PH bank (from the device GPIO map). */
#define PH6_BIT		(1u << 6)
#define PH7_BIT		(1u << 7)
#define PH13_BIT	(1u << 13)
#define PH18_BIT	(1u << 18)
#define PHASE_ALL	(PH6_BIT | PH7_BIT | PH13_BIT | PH18_BIT)

/* One electrical revolution: phase 0,1,2,3 = PH13, PH18, PH6, PH7. */
static const u32 phase_seq[4] = { PH13_BIT, PH18_BIT, PH6_BIT, PH7_BIT };

#define SWEEP_STEPS	200		/* steps per direction (tune for travel) */
#define STEP_US		2200		/* per-step dwell; a stepper needs a few ms */

static void __iomem *h726_pio;

static int h726_map(void)
{
	if (!h726_pio)
		h726_pio = ioremap(PIO_BASE, PIO_SIZE);
	return h726_pio ? 0 : -ENOMEM;
}

static void h726_motor_init(void)
{
	u32 v;

	v = readl(h726_pio + PH_CFG0);		/* PH6, PH7 -> output (0x1) */
	v &= ~0xFF000000u;
	v |= (0x1u << 24) | (0x1u << 28);
	writel(v, h726_pio + PH_CFG0);

	v = readl(h726_pio + PH_CFG1);		/* PH13 -> output */
	v &= ~0x00F00000u;
	v |= (0x1u << 20);
	writel(v, h726_pio + PH_CFG1);

	v = readl(h726_pio + PH_CFG2);		/* PH18 -> output */
	v &= ~0x00000F00u;
	v |= (0x1u << 8);
	writel(v, h726_pio + PH_CFG2);
}

static void h726_motor_drive(u32 phase_mask)
{
	u32 v = readl(h726_pio + PH_DAT);

	v = (v & ~PHASE_ALL) | phase_mask;	/* only the four motor bits */
	writel(v, h726_pio + PH_DAT);
}

static void h726_sweep(bool forward)
{
	int i, idx;

	for (i = 0; i < SWEEP_STEPS; i++) {
		idx = forward ? (i & 3) : (3 - (i & 3));
		h726_motor_drive(phase_seq[idx]);
		usleep_range(STEP_US, STEP_US + 300);
	}
}

/* One visible focus breath: sweep out, sweep back, de-energize. */
static void h726_breath(void)
{
	h726_sweep(true);
	h726_sweep(false);
	h726_motor_drive(0);			/* coils off between breaths */
}

static void h726_beacon(unsigned int breaths)
{
	unsigned int i;

	for (i = 0; i < breaths; i++) {
		h726_breath();
		msleep(400);
	}
}

/* CP: reached the init thread. Two focus breaths. */
static int __init h726_cp_init(void)
{
	if (h726_map())
		return 0;
	h726_motor_init();
	h726_beacon(2);
	return 0;
}
early_initcall(h726_cp_init);

/* Steady heartbeat: kernel fully alive (scheduler + timekeeping running). */
static int h726_heartbeat_fn(void *unused)
{
	while (!kthread_should_stop()) {
		h726_breath();
		msleep(1200);
	}
	return 0;
}

static int __init h726_heartbeat_start(void)
{
	if (h726_map())
		return 0;
	h726_motor_init();
	kthread_run(h726_heartbeat_fn, NULL, "h726-heartbeat");
	return 0;
}
late_initcall(h726_heartbeat_start);
