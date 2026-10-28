#!/bin/bash
# Enable the kernel options the HY300 Ultra (H726 / sun50iw15) needs, on top
# of the generic arm64 defconfig. Adapted from the H713 port's configure
# script (github.com/shift/sun50iw12p1-research), minus the MIPS display
# co-processor (the H726 has none), plus this board's PMIC, panel and sensors.
#
# Usage: run from inside the kernel source tree (after `make defconfig`).
set -e
C=./scripts/config

# Platform
$C --enable CONFIG_ARCH_SUNXI
$C --enable CONFIG_MACH_SUN50I

# GPU (Mali-Midgard via Panfrost) and IOMMU
$C --enable CONFIG_DRM
$C --enable CONFIG_DRM_PANFROST
$C --enable CONFIG_IOMMU_SUPPORT
$C --enable CONFIG_SUN50I_IOMMU

# Clocks / pinctrl. NOTE: mainline has no sun50iw15 CCU/pinctrl yet; the
# H6 drivers are enabled as a starting point but will not fully bind
# (see kernel/README.md). This is the #1 bring-up task.
$C --enable CONFIG_PINCTRL_SUN50I_H6
$C --enable CONFIG_PINCTRL_SUN50I_H6_R

# Storage
$C --enable CONFIG_MMC
$C --enable CONFIG_MMC_SUNXI

# DVFS / thermal
$C --enable CONFIG_CPUFREQ_DT
$C --enable CONFIG_THERMAL
$C --enable CONFIG_SUN8I_THERMAL
$C --enable CONFIG_CPU_FREQ_GOV_ONDEMAND

# Buses and sensors
$C --enable CONFIG_I2C
$C --enable CONFIG_I2C_MV64XXX
$C --enable CONFIG_SPI
$C --enable CONFIG_SPI_SUN6I
$C --enable CONFIG_IIO
$C --enable CONFIG_KXCJK1013   # kxtj3 accelerometer
$C --enable CONFIG_IIO_ST_LSM6DSX

# PMIC (AXP1530)
$C --enable CONFIG_MFD_AXP20X
$C --enable CONFIG_MFD_AXP20X_I2C
$C --enable CONFIG_REGULATOR
$C --enable CONFIG_REGULATOR_AXP20X

# PWM (fan, backlight)
$C --enable CONFIG_PWM
$C --enable CONFIG_PWM_SUN4I
$C --enable CONFIG_SENSORS_PWM_FAN

# Ethernet and USB
$C --enable CONFIG_STMMAC_ETH
$C --enable CONFIG_DWMAC_SUN8I
$C --enable CONFIG_USB
$C --enable CONFIG_USB_EHCI_HCD
$C --enable CONFIG_USB_OHCI_HCD

# Crypto engine
$C --enable CONFIG_CRYPTO_DEV_SUN8I_CE

# Audio
$C --enable CONFIG_SOUND
$C --enable CONFIG_SND
$C --enable CONFIG_SND_SOC

# WiFi stack (AIC8800 driver is out of tree)
$C --enable CONFIG_CFG80211
$C --enable CONFIG_MAC80211

# IR remote
$C --enable CONFIG_RC_CORE
$C --enable CONFIG_IR_GPIO_CIR

# No-serial bring-up aids (Phase B3): crash logs survive to pstore.
$C --enable CONFIG_PSTORE
$C --enable CONFIG_PSTORE_RAM

echo "H726 kernel configuration applied."
