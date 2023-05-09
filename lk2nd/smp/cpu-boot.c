// SPDX-License-Identifier: BSD-3-Clause
/* Copyright (c) 2021-2022, Stephan Gerhold <stephan@gerhold.net> */

#include <bits.h>
#include <debug.h>
#include <platform/timer.h>
#include <scm.h>

#include <libfdt.h>
#include <lk2nd/util/lkfdt.h>

#include "cpu-boot.h"

#define QCOM_SCM_BOOT_SET_ADDR		0x01
#define QCOM_SCM_BOOT_FLAG_COLD_ALL	(0 | BIT(0) | BIT(3) | BIT(5))
#define QCOM_SCM_BOOT_SET_ADDR_MC	0x11
#define QCOM_SCM_BOOT_MC_FLAG_AARCH64	BIT(0)
#define QCOM_SCM_BOOT_MC_FLAG_COLDBOOT	BIT(1)
#define QCOM_SCM_BOOT_MC_FLAG_WARMBOOT	BIT(2)

int cpu_boot_set_addr(uintptr_t addr, bool arm64)
{
	uint32_t aarch64 = arm64 ? QCOM_SCM_BOOT_MC_FLAG_AARCH64 : 0;
	scmcall_arg arg = {
		.x0 = MAKE_SIP_SCM_CMD(SCM_SVC_BOOT, QCOM_SCM_BOOT_SET_ADDR_MC),
		.x1 = MAKE_SCM_ARGS(6),
		.x2 = addr,
		.x3 = ~0UL, .x4 = ~0UL, .x5 = {~0UL, ~0UL, /* All CPUs */
		aarch64 | QCOM_SCM_BOOT_MC_FLAG_COLDBOOT},
	};

	if (is_scm_armv8_support())
		return scm_call2(&arg, NULL);

	dprintf(INFO, "Falling back to legacy QCOM_SCM_BOOT_SET_ADDR call\n");
	return scm_call_atomic2(SCM_SVC_BOOT, QCOM_SCM_BOOT_SET_ADDR,
				QCOM_SCM_BOOT_FLAG_COLD_ALL, addr);
}

static inline uint32_t read_mpidr(void)
{
	uint32_t res;
	__asm__ ("mrc p15, 0, %0, c0, c0, 5" : "=r" (res));
	return BITS(res, 23, 0);
}

/**
 * @brief This function used to read value by index of specified property
 * 
 * Example of node:
 * @code
 *	somenode {
 *		someproperty = <1 2 3 4>;
 *	};
 * @endcode
 * 
 * So, to get second value(2) of this `someproperty` call this function
 * like this:
 * 
 * @code
 * val = read_phandle_value_indexed(dtb, node, "someproperty", 1)
 * @endcode
 * 
 * @param dtb The pointer to dtb
 * @param node phandle of node, where to search property
 * @param name property name
 * @param index if property value is tuple, index specifies which value
 * 		  of tuple to return
 * @return value of specified property by index or 0 if not possible
 */
static uint32_t read_phandle_value_indexed(const void *dtb, int node, 
		const char *name, int index)
{
	const fdt32_t *val;
	int len;

	val = fdt_getprop(dtb, node, name, &len);
	if (len < (int)sizeof(*val)) {
		dprintf(CRITICAL, "Cannot read %s property of node: %d\n",
			name, len);
		return 0;
	}
	return fdt32_to_cpu(*(val + index));
}

/**
 * @brief This function return reg value by index of specified node
 * @param dtb The pointer to dtb
 * @param node phandle of node, where to search target node
 * @param name target node name
 * @param index index of which reg tuple value to return
 * 
 * Example of node:
 * @code
 *	l2ccc_0: clock-controller@f900d000 {
 *		compatible = "qcom,8994-l2ccc";
 *		reg = <0xf900d000 0x1000>;
 *		qcom,vctl-node = <&cluster0_spm>;
 *	};
 *	
 *	cluster0_spm: qcom,spm@f9012000 {
 *			compatible = "qcom,spm-v2";
 *			#address-cells = <1>;
 *			#size-cells = <1>;
 *			reg = <0xf9012000 0x1000>,
 *				<0xf900d210 0x8>;
 *	};
 * @endcode
 * 
 * So, to get second reg address of `cluster0_spm`(0xf900d210), call function
 * like this:
 * 
 * @code
 * val = read_phandle_reg_indexed(dtb, node, "qcom,vctl-node", 1)
 * @endcode
 * 
 * where `node` is on `l2ccc_0` now
 * 
 * @note index specifies what tuple to use, not value
 * @note `node` must contain node with subnode called `name`
 * 
 * @return value of specified node reg by index or 0 if not possible
 */
static uint32_t read_phandle_reg_indexed(const void *dtb, int node,
		const char *prop, int index)
{
	/* 
	 * used_index is index multiplied by 2 because we want to use the first
	 * value from the second tuple, not the second value from the first tuple
	 */
	int target, used_index = index * 2;

	target = lkfdt_lookup_phandle(dtb, node, prop);
	if (target < 0) {
		dprintf(CRITICAL, "Cannot find %s node in %s: %d\n",
			prop, fdt_get_name(dtb, node, NULL), target);
		return 0;
	}
	return read_phandle_value_indexed(dtb, target, "reg", used_index);
}

/**
 * @brief The same as `read_phandle_value_indexed` but with default index 0
 * @see read_phandle_value_indexed
 */
static uint32_t read_phandle_value(const void *dtb, int node, const char *name)
{
	return read_phandle_value_indexed(dtb, node, name, 0);
}

/**
 * @brief The same as `read_phandle_reg_indexed` but with default index 0
 * @see read_phandle_reg_indexed
 */
static uint32_t read_phandle_reg(const void *dtb, int node, const char *prop)
{
	return read_phandle_reg_indexed(dtb, node, prop, 0);
}

bool cpu_boot(const void *dtb, int node, uint32_t mpidr)
{
	uint32_t acc, extra_reg __UNUSED;

	if (mpidr == read_mpidr()) {
		dprintf(INFO, "Skipping boot of current CPU (%x)\n", mpidr);
		return true;
	}

	/* Boot the CPU core using registers in the ACC node */
	acc = read_phandle_reg(dtb, node, "qcom,acc");
	if (!acc)
		return false;

	dprintf(INFO, "Booting CPU%x @ %#08x\n", mpidr, acc);

#if CPU_BOOT_CORTEX_A
	/*
	 * The CPU clock happens to point to the "APCS" node that also controls
	 * the power signals for the L2 cache. The address does not have to be
	 * present since on SoCs with a single CPU cluster the L2 cache should
	 * already be powered on and active.
	 */
	extra_reg = read_phandle_reg(dtb, node, "clocks");
	cpu_boot_cortex_a(acc, extra_reg);
#elif CPU_BOOT_KPSSV1
	extra_reg = read_phandle_reg(dtb, node, "qcom,saw");
	if (!extra_reg)
		return false;
	cpu_boot_kpssv1(acc, extra_reg);
#elif CPU_BOOT_KPSSV2
	node = lkfdt_lookup_phandle(dtb, node, "next-level-cache");
	if (node < 0) {
		dprintf(CRITICAL, "Cannot find CPU next-level-cache: %d\n", node);
		return false;
	}
	extra_reg = read_phandle_reg(dtb, node, "qcom,saw");
	if (!extra_reg)
		return false;
	cpu_boot_kpssv2(acc, extra_reg);
#else
#error Unsupported CPU boot method!
#endif

	/* Give CPU some time to boot */
	udelay(100);
	return true;
}
