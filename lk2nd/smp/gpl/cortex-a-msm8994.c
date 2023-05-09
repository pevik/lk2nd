// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2023, Eugene Lepshy <fekz115@gmail.com> */

#include <arch/defines.h>
#include <bits.h>
#include <debug.h>
#include <kernel/thread.h>
#include <platform/timer.h>
#include <reg.h>

#include "../cpu-boot.h"

#define CPU_PWR_CTL			0x4
#define APC_PWR_GATE_CTL	0x14

#define L1_RST_DIS			0x284

#define L2_VREG_CTL			0x1c
#define L2_PWR_CTL			0x14
#define L2_PWR_CTL_OVERRIDE	0xc
#define L2_PWR_STATUS_L2_HS_STS_MSM8994	(BIT(9) | BIT(28))

/**
 * @brief delay for voltage to settle on the core 
 */
#define REGULATOR_SETUP_VOLTAGE_TIMEOUT 2000

/**
 * @brief Power on cpu rail before turning on core
 * @param vctl_base_0 first `qcom,vctl-node` reg address
 * @param vctl_base_1 second `qcom,vctl-node` reg address
 * @param vctl_val The value to be set on the rail
 */
static void msm_spm_turn_on_cpu_rail(uint32_t vctl_base_0, uint32_t vctl_base_1,
		unsigned int vctl_val)
{
	if (vctl_base_1) {
		/*
		 * Program Q2S to disable SPM legacy mode and ignore Q2S
		 * channel requests.
		 * bit[1] = qchannel_ignore = 1
		 * bit[2] = spm_legacy_mode = 0
		 */
		writel(0x2, vctl_base_1);
		dsb();
	}

	/* Set the CPU supply regulator voltage */
	vctl_val = (vctl_val & 0xFF);
	writel(vctl_val, vctl_base_0 + L2_VREG_CTL);
	dsb();
	udelay(REGULATOR_SETUP_VOLTAGE_TIMEOUT);

	/* Enable the CPU supply regulator*/
	vctl_val = 0x30080;
	writel(vctl_val, vctl_base_0 + L2_VREG_CTL);
	dsb();
	udelay(REGULATOR_SETUP_VOLTAGE_TIMEOUT);
}

/**
 * @brief This function used to enable l2 cache
 * 
 * As l2 cache for first(boot) cluster enabled by lk1st,
 * this function skips it and work only for second cluster
 * 
 * @param l2ccc_base value of l2 clock controller reg
 * @param vctl_base_0 first `qcom,vctl-node` reg address
 * @param vctl_base_1 second `qcom,vctl-node` reg address
 * @param vctl_val The value to be set on the rail
 * @note Function has a check if cache at `l2ccc_base` already enabled
 */
static void power_on_l2_cache_msm8994(uint32_t l2ccc_base, uint32_t vctl_base_0,
		uint32_t vctl_base_1, uint32_t vctl_val) {
	/* Skip if cluster L2 is already powered on */
	if (readl(l2ccc_base + L2_PWR_CTL) & L2_PWR_STATUS_L2_HS_STS_MSM8994)
		return;

	msm_spm_turn_on_cpu_rail(vctl_base_0, vctl_base_1, vctl_val);

	dprintf(INFO, "Powering on L2 cache @ %#08x\n", l2ccc_base);

	enter_critical_section();

	/* Enable L1 invalidation by h/w */
	writel(0x00000000, l2ccc_base + L1_RST_DIS);
	dsb();

	/* Assert PRESETDBGn */
	writel(0x00400000 , l2ccc_base + L2_PWR_CTL_OVERRIDE);
	dsb();

	/* Close L2/SCU Logic GDHS and power up the cache */
	writel(0x00029716 , l2ccc_base + L2_PWR_CTL);
	dsb();
	udelay(8);

	/* De-assert L2/SCU memory Clamp */
	writel(0x00023716 , l2ccc_base + L2_PWR_CTL);
	dsb();

	/* Wakeup L2/SCU RAMs by deasserting sleep signals */
	writel(0x0002371E , l2ccc_base + L2_PWR_CTL);
	dsb();
	udelay(8);

	/* Un-gate clock and wait for sequential waking up
	 * of L2 rams with a delay of 2*X0 cycles
	 */
	writel(0x0002371C , l2ccc_base + L2_PWR_CTL);
	dsb();
	udelay(4);

	/* De-assert L2/SCU logic clamp */
	writel(0x0002361C , l2ccc_base + L2_PWR_CTL);
	dsb();
	udelay(2);

	/* De-assert L2/SCU logic reset */
	writel(0x00022218 , l2ccc_base + L2_PWR_CTL);
	dsb();
	udelay(4);

	/* Turn on the PMIC_APC */
	writel(0x10022218 , l2ccc_base + L2_PWR_CTL);
	dsb();

	/* De-assert PRESETDBGn */
	writel(0x00000000 , l2ccc_base + L2_PWR_CTL_OVERRIDE);
	dsb();
	exit_critical_section();
}

void cpu_boot_cortex_a_msm8994(uint32_t base, uint32_t l2ccc_base,
		uint32_t vctl_base_0, uint32_t vctl_base_1, uint32_t vctl_val)
{
	if (l2ccc_base)
		power_on_l2_cache_msm8994(l2ccc_base, vctl_base_0, vctl_base_1, vctl_val);

	enter_critical_section();

	/* Assert head switch enable few */
	writel(0x00000001, base + APC_PWR_GATE_CTL);
	dsb();
	udelay(1);

	/* Assert head switch enable rest */
	writel(0x00000003, base + APC_PWR_GATE_CTL);
	dsb();
	udelay(1);

	/* De-assert coremem clamp. This is asserted by default */
	writel(0x00000079, base + CPU_PWR_CTL);
	dsb();
	udelay(2);

	/* Close coremem array gdhs */
	writel(0x0000007D, base + CPU_PWR_CTL);
	dsb();
	udelay(2);

	/* De-assert clamp */
	writel(0x0000003D, base + CPU_PWR_CTL);
	dsb();

	/* De-assert clamp */
	writel(0x0000003C, base + CPU_PWR_CTL);
	dsb();
	udelay(1);

	/* De-assert core0 reset */
	writel(0x0000000C, base + CPU_PWR_CTL);
	dsb();

	/* Assert PWRDUP */
	writel(0x0000008C, base + CPU_PWR_CTL);
	dsb();

	exit_critical_section();
}