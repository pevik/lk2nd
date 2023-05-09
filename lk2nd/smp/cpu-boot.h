/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LK2ND_SMP_CPU_BOOT_H
#define LK2ND_SMP_CPU_BOOT_H

int cpu_boot_set_addr(uintptr_t addr, bool arm64);

bool cpu_boot(const void *dtb, int node, uint32_t mpidr);

/**
 * @brief This function boot cpus for msm8994/2 if firmware doesn't
 * support pcsi
 * 
 * @note cpu node must contain `next-level-cache`
 * @note cache node must contain `power-domain`
 * @note power-domain node must contain `qcom,vctl-node`
 * 		 and might contain `qcom,vctl-val`
 * 
 * @return true if core successfully booted
 */
bool boot_msm8994_cpu(const void *dtb, int acc, int node);
void cpu_boot_cortex_a(uint32_t base, uint32_t apcs_base);

/**
 * @brief This function enables core with the address stored in `base`
 * 
 * As l2 cache for first(boot) cluster enabled by lk1st,
 * this function skips it and work only for second cluster
 * 
 * @param base value of current cpu reg
 * @param l2ccc_base value of l2 clock controller reg
 * @param vctl_base_0 first `qcom,vctl-node` reg address
 * @param vctl_base_1 second `qcom,vctl-node` reg address
 * @param vctl_val The value to be set on the rail
 */
void cpu_boot_cortex_a_msm8994(uint32_t base, uint32_t l2ccc_base,
        uint32_t vctl_base_0, uint32_t vctl_base_1, uint32_t vctl_val);
void cpu_boot_kpssv1(uint32_t reg, uint32_t saw_reg);
void cpu_boot_kpssv2(uint32_t reg, uint32_t l2_saw_base);

#endif /* LK2ND_SMP_CPU_BOOT_H */
