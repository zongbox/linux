/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 SiFive
 * Copyright (C) 2018 Andes Technology Corporation
 *
 */

#ifndef _ASM_RISCV_PERF_EVENT_H
#define _ASM_RISCV_PERF_EVENT_H

#include <linux/perf_event.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>

#ifdef CONFIG_RISCV_BASE_PMU
#define RISCV_BASE_COUNTERS	2
#define RISCV_EVENT_COUNTERS	29
#define RISCV_TOTAL_COUNTERS	(RISCV_BASE_COUNTERS + RISCV_EVENT_COUNTERS)

#define RISCV_DEFAULT_WIDTH_COUNTER	64

/*
 * According to the spec, an implementation can support counter up to
 * mhpmcounter31, but many high-end processors has at most 6 general
 * PMCs, we give the definition to MHPMCOUNTER8 here.
 */
#define RISCV_PMU_CYCLE			0
#define RISCV_PMU_INSTRET		2
#define RISCV_PMU_HPMCOUNTER3		3
#define RISCV_PMU_HPMCOUNTER4		4
#define RISCV_PMU_HPMCOUNTER5		5
#define RISCV_PMU_HPMCOUNTER6		6
#define RISCV_PMU_HPMCOUNTER7		7
#define RISCV_PMU_HPMCOUNTER8		8

#define RISCV_PMU_HPMCOUNTER_FIRST	3
#define RISCV_PMU_HPMCOUNTER_LAST					\
	(RISCV_PMU_HPMCOUNTER_FIRST + riscv_pmu.num_event_cntr - 1)

#define RISCV_OP_UNSUPP			(-EOPNOTSUPP)

#define RISCV_MAP_ALL_UNSUPPORTED					\
	[0 ... PERF_COUNT_HW_MAX - 1] = RISCV_OP_UNSUPP

#define C(x) PERF_COUNT_HW_CACHE_##x

#define RISCV_CACHE_MAP_ALL_UNSUPPORTED					\
[0 ... C(MAX) - 1] = {							\
	[0 ... C(OP_MAX) - 1] = {					\
		[0 ... C(RESULT_MAX) - 1] = RISCV_OP_UNSUPP,		\
	},								\
}

/* Hardware cache event encoding */
#define PERF_HW_CACHE_TYPE		0
#define PERF_HW_CACHE_OP		8
#define PERF_HW_CACHE_RESULT		16
#define PERF_HW_CACHE_MASK		0xff

/* config_base encoding */
#define RISCV_PMU_TYPE_MASK		0x3
#define RISCV_PMU_TYPE_BASE		0x1
#define RISCV_PMU_TYPE_EVENT		0x2
#define RISCV_PMU_EXCLUDE_MASK		0xc
#define RISCV_PMU_EXCLUDE_USER		0x3
#define RISCV_PMU_EXCLUDE_KERNEL	0x4

/*
 * Currently, machine-mode supports emulation of mhpmeventN. Setting mhpmeventN
 * to raise an illegal instruction exception to set event types in machine-mode.
 * Eventually, we should set event types through standard SBI call or the shadow
 * CSRs of supervisor-mode, because it is weird for writing CSR of machine-mode
 * explicitly in supervisor-mode. These macro should be removed in the future.
 */
#define CSR_MHPMEVENT3	0x323
#define CSR_MHPMEVENT4	0x324
#define CSR_MHPMEVENT5	0x325
#define CSR_MHPMEVENT6	0x326
#define CSR_MHPMEVENT7	0x327
#define CSR_MHPMEVENT8	0x328

#endif
#ifdef CONFIG_PERF_EVENTS
#define perf_arch_bpf_user_pt_regs(regs) (struct user_regs_struct *)regs
#endif

#endif /* _ASM_RISCV_PERF_EVENT_H */
