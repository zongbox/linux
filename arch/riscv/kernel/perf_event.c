/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2008 Thomas Gleixner <tglx@linutronix.de>
 * Copyright (C) 2008-2009 Red Hat, Inc., Ingo Molnar
 * Copyright (C) 2009 Jaswinder Singh Rajput
 * Copyright (C) 2009 Advanced Micro Devices, Inc., Robert Richter
 * Copyright (C) 2008-2009 Red Hat, Inc., Peter Zijlstra
 * Copyright (C) 2009 Intel Corporation, <markus.t.metzger@intel.com>
 * Copyright (C) 2009 Google, Inc., Stephane Eranian
 * Copyright 2014 Tilera Corporation. All Rights Reserved.
 * Copyright (C) 2018 Andes Technology Corporation
 *
 * Perf_events support for RISC-V platforms.
 *
 * Since the spec. (as of now, Priv-Spec 1.10) does not provide enough
 * functionality for perf event to fully work, this file provides
 * the very basic framework only.
 *
 * For platform portings, please check Documentations/riscv/pmu.txt.
 *
 * The Copyright line includes x86 and tile ones.
 */

#include <linux/kprobes.h>
#include <linux/kernel.h>
#include <linux/kdebug.h>
#include <linux/mutex.h>
#include <linux/bitmap.h>
#include <linux/irq.h>
#include <linux/perf_event.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <asm/perf_event.h>

static const struct riscv_pmu *riscv_pmu __read_mostly;
static DEFINE_PER_CPU(struct cpu_hw_events, cpu_hw_events);

/*
 * Hardware & cache maps and their methods
 */

static const int riscv_hw_event_map[] = {
	[PERF_COUNT_HW_CPU_CYCLES]		= RISCV_PMU_CYCLE,
	[PERF_COUNT_HW_INSTRUCTIONS]		= RISCV_PMU_INSTRET,
	[PERF_COUNT_HW_CACHE_REFERENCES]	= RISCV_OP_UNSUPP,
	[PERF_COUNT_HW_CACHE_MISSES]		= RISCV_OP_UNSUPP,
	[PERF_COUNT_HW_BRANCH_INSTRUCTIONS]	= RISCV_OP_UNSUPP,
	[PERF_COUNT_HW_BRANCH_MISSES]		= RISCV_OP_UNSUPP,
	[PERF_COUNT_HW_BUS_CYCLES]		= RISCV_OP_UNSUPP,
};

#define C(x) PERF_COUNT_HW_CACHE_##x
static const int riscv_cache_event_map[PERF_COUNT_HW_CACHE_MAX]
[PERF_COUNT_HW_CACHE_OP_MAX]
[PERF_COUNT_HW_CACHE_RESULT_MAX] = {
	[C(L1D)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
	},
	[C(L1I)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
	},
	[C(LL)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
	},
	[C(DTLB)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)] =  RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] =  RISCV_OP_UNSUPP,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
	},
	[C(ITLB)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
	},
	[C(BPU)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)] = RISCV_OP_UNSUPP,
			[C(RESULT_MISS)] = RISCV_OP_UNSUPP,
		},
	},
};

/*
 * Methods for checking and getting PMU information
 */

static inline int is_base_counter(int idx)
{
	return (idx == RISCV_PMU_CYCLE || idx == RISCV_PMU_INSTRET);
}

static inline int is_event_counter(int idx)
{
	return (idx >= RISCV_PMU_HPMCOUNTER_FIRST &&
		idx <= RISCV_PMU_HPMCOUNTER_LAST);
}

static inline int get_available_counter(struct perf_event *event)
{
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);
	struct hw_perf_event *hwc = &event->hw;
	unsigned long config_base = hwc->config_base & RISCV_PMU_TYPE_MASK;
	unsigned long mask;
	int ret;

	switch (config_base) {
	case RISCV_PMU_TYPE_BASE:
		ret = hwc->config;
		if (WARN_ON_ONCE(!is_base_counter(ret)))
			return -ENOSPC;
		break;
	case RISCV_PMU_TYPE_EVENT:
		mask = ~cpuc->used_cntr_mask;
		ret = find_next_bit(&mask, RISCV_PMU_HPMCOUNTER_LAST, 3);
		if (WARN_ON_ONCE(!is_event_counter(ret)))
			return -ENOSPC;
		break;
	default:
		return -ENOENT;
	}

	__set_bit(ret, &cpuc->used_cntr_mask);

	return ret;
}

/*
 * Map generic hardware event
 */
static int riscv_map_hw_event(u64 config)
{
	if (config >= riscv_pmu->max_events)
		return -EINVAL;

	return riscv_pmu->hw_events[config];
}

/*
 * Map generic hardware cache event
 */
static int riscv_map_cache_event(u64 config)
{
	unsigned int type, op, result;
	int ret;

	type	= (config >> PERF_HW_CACHE_TYPE) & PERF_HW_CACHE_MASK;
	op	= (config >> PERF_HW_CACHE_OP) & PERF_HW_CACHE_MASK;
	result	= (config >> PERF_HW_CACHE_RESULT) & PERF_HW_CACHE_MASK;

	if (type >= PERF_COUNT_HW_CACHE_MAX ||
	    op >= PERF_COUNT_HW_CACHE_OP_MAX ||
	    result >= PERF_COUNT_HW_CACHE_RESULT_MAX)
		return -EINVAL;

	ret = riscv_cache_event_map[type][op][result];
	if (ret == RISCV_OP_UNSUPP)
		return -EINVAL;

	return ret == RISCV_OP_UNSUPP ? -ENOENT : ret;
}

/*
 * Low-level functions: reading/writing counters
 */

static inline u64 read_counter(int idx)
{
	u64 val = 0;

	switch (idx) {
	case RISCV_PMU_CYCLE:
		val = csr_read(CSR_CYCLE);
		break;
	case RISCV_PMU_INSTRET:
		val = csr_read(CSR_INSTRET);
		break;
	case RISCV_PMU_HPMCOUNTER3:
		val = csr_read(CSR_HPMCOUNTER3);
		break;
	case RISCV_PMU_HPMCOUNTER4:
		val = csr_read(CSR_HPMCOUNTER4);
		break;
	case RISCV_PMU_HPMCOUNTER5:
		val = csr_read(CSR_HPMCOUNTER5);
		break;
	case RISCV_PMU_HPMCOUNTER6:
		val = csr_read(CSR_HPMCOUNTER6);
		break;
	case RISCV_PMU_HPMCOUNTER7:
		val = csr_read(CSR_HPMCOUNTER7);
		break;
	case RISCV_PMU_HPMCOUNTER8:
		val = csr_read(CSR_HPMCOUNTER8);
		break;
	default:
		WARN_ON_ONCE(idx < RISCV_PMU_CYCLE ||
			     idx > RISCV_TOTAL_COUNTERS);
		return -EINVAL;
	}

	return val;
}

static inline void write_counter(int idx, u64 value)
{
	/* currently not supported */
	WARN_ON_ONCE(1);
}

static inline void write_event(int idx, u64 value)
{
	/* TODO: We shouldn't write CSR of m-mode explicitly here. Ideally,
	 * it need to set the event selector by SBI call or the s-mode
	 * shadow CSRs of them. Exploit illegal instruction exception to
	 * emulate mhpmcounterN access in m-mode.
	 */
	switch (idx) {
	case RISCV_PMU_HPMCOUNTER3:
		csr_write(CSR_MHPMEVENT3, value);
		break;
	case RISCV_PMU_HPMCOUNTER4:
		csr_write(CSR_MHPMEVENT4, value);
		break;
	case RISCV_PMU_HPMCOUNTER5:
		csr_write(CSR_MHPMEVENT5, value);
		break;
	case RISCV_PMU_HPMCOUNTER6:
		csr_write(CSR_MHPMEVENT6, value);
		break;
	case RISCV_PMU_HPMCOUNTER7:
		csr_write(CSR_MHPMEVENT7, value);
		break;
	case RISCV_PMU_HPMCOUNTER8:
		csr_write(CSR_MHPMEVENT8, value);
		break;
	default:
		WARN_ON_ONCE(idx < RISCV_PMU_HPMCOUNTER3 ||
			     idx > RISCV_TOTAL_COUNTERS);
		return;
	}
}

/*
 * Enable and disable event counters
 */

static inline void riscv_pmu_enable_event(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	if (is_event_counter(idx))
		write_event(idx, hwc->config);

	/*
	 * Since we cannot write to counters, this serves as an initialization
	 * to the delta-mechanism in pmu->read(); otherwise, the delta would be
	 * wrong when pmu->read is called for the first time.
	 */
	local64_set(&hwc->prev_count, read_counter(hwc->idx));
}

static inline void riscv_pmu_disable_event(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	if (is_event_counter(idx))
		write_event(idx, 0);
}

/*
 * pmu->read: read and update the counter
 *
 * Other architectures' implementation often have a xxx_perf_event_update
 * routine, which can return counter values when called in the IRQ, but
 * return void when being called by the pmu->read method.
 */
static void riscv_pmu_read(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	u64 prev_raw_count, new_raw_count;
	u64 oldval;
	int idx = hwc->idx;
	u64 delta;

	do {
		prev_raw_count = local64_read(&hwc->prev_count);
		new_raw_count = read_counter(idx);

		oldval = local64_cmpxchg(&hwc->prev_count, prev_raw_count,
					 new_raw_count);
	} while (oldval != prev_raw_count);

	/*
	 * delta is the value to update the counter we maintain in the kernel.
	 */
	delta = (new_raw_count - prev_raw_count) &
		((1ULL << riscv_pmu->counter_width) - 1);

	local64_add(delta, &event->count);
	/*
	 * Something like local64_sub(delta, &hwc->period_left) here is
	 * needed if there is an interrupt for perf.
	 */
}

/*
 * State transition functions:
 *
 * stop()/start() & add()/del()
 */

/*
 * pmu->stop: stop the counter
 */
static void riscv_pmu_stop(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;

	if (WARN_ON_ONCE(hwc->idx == -1))
		return;

	riscv_pmu_disable_event(event);

	WARN_ON_ONCE(hwc->state & PERF_HES_STOPPED);
	hwc->state |= PERF_HES_STOPPED;

	if ((flags & PERF_EF_UPDATE) && !(hwc->state & PERF_HES_UPTODATE)) {
		riscv_pmu->pmu->read(event);
		hwc->state |= PERF_HES_UPTODATE;
	}
}

/*
 * pmu->start: start the event.
 */
static void riscv_pmu_start(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;

	if (WARN_ON_ONCE(!(event->hw.state & PERF_HES_STOPPED)))
		return;

	if (WARN_ON_ONCE(hwc->idx == -1))
		return;

	if (flags & PERF_EF_RELOAD) {
		WARN_ON_ONCE(!(event->hw.state & PERF_HES_UPTODATE));

		/*
		 * Set the counter to the period to the next interrupt here,
		 * if you have any.
		 */
	}

	hwc->state = 0;

	riscv_pmu_enable_event(event);

	perf_event_update_userpage(event);
}

/*
 * pmu->add: add the event to PMU.
 */
static int riscv_pmu_add(struct perf_event *event, int flags)
{
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);
	struct hw_perf_event *hwc = &event->hw;
	int count_idx;

	if (cpuc->n_events == riscv_pmu->num_counters)
		return -ENOSPC;

	count_idx = get_available_counter(event);
	if (count_idx < 0)
		return -ENOSPC;

	cpuc->n_events++;
	hwc->idx = count_idx;
	cpuc->events[hwc->idx] = event;

	hwc->state = PERF_HES_UPTODATE | PERF_HES_STOPPED;

	if (flags & PERF_EF_START)
		riscv_pmu->pmu->start(event, PERF_EF_RELOAD);

	return 0;
}

/*
 * pmu->del: delete the event from PMU.
 */
static void riscv_pmu_del(struct perf_event *event, int flags)
{
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);
	struct hw_perf_event *hwc = &event->hw;

	cpuc->n_events--;
	__clear_bit(hwc->idx, &cpuc->used_cntr_mask);

	cpuc->events[hwc->idx] = NULL;
	riscv_pmu->pmu->stop(event, PERF_EF_UPDATE);
	perf_event_update_userpage(event);
}

/*
 * Interrupt: a skeletion for reference.
 */

static DEFINE_MUTEX(pmc_reserve_mutex);

static irqreturn_t riscv_base_pmu_handle_irq(int irq_num, void *dev)
{
	return IRQ_NONE;
}

static int reserve_pmc_hardware(void)
{
	int err = 0;

	mutex_lock(&pmc_reserve_mutex);
	if (riscv_pmu->irq >= 0 && riscv_pmu->handle_irq) {
		err = request_irq(riscv_pmu->irq, riscv_pmu->handle_irq,
				  IRQF_PERCPU, "riscv-base-perf", NULL);
	}
	mutex_unlock(&pmc_reserve_mutex);

	return err;
}

static void release_pmc_hardware(void)
{
	mutex_lock(&pmc_reserve_mutex);
	if (riscv_pmu->irq >= 0)
		free_irq(riscv_pmu->irq, NULL);
	mutex_unlock(&pmc_reserve_mutex);
}

/*
 * Event Initialization/Finalization
 */

static atomic_t riscv_active_events = ATOMIC_INIT(0);

static void riscv_event_destroy(struct perf_event *event)
{
	if (atomic_dec_return(&riscv_active_events) == 0)
		release_pmc_hardware();
}

static int riscv_event_init(struct perf_event *event)
{
	struct perf_event_attr *attr = &event->attr;
	struct hw_perf_event *hwc = &event->hw;
	unsigned long config_base = 0;
	int err;
	int code;

	if (atomic_inc_return(&riscv_active_events) == 1) {
		err = reserve_pmc_hardware();

		if (err) {
			pr_warn("PMC hardware not available\n");
			atomic_dec(&riscv_active_events);
			return -EBUSY;
		}
	}

	switch (event->attr.type) {
	case PERF_TYPE_HARDWARE:
		code = riscv_pmu->map_hw_event(attr->config);
		break;
	case PERF_TYPE_HW_CACHE:
		code = riscv_pmu->map_cache_event(attr->config);
		break;
	case PERF_TYPE_RAW:
		code = attr->config;
		break;
	default:
		return -ENOENT;
	}

	if (is_base_counter(code))
		config_base |= RISCV_PMU_TYPE_BASE;
	else
		config_base |= RISCV_PMU_TYPE_EVENT;

	event->destroy = riscv_event_destroy;
	if (code < 0) {
		event->destroy(event);
		return code;
	}

	/*
	 * idx is set to -1 because the index of a general event should not be
	 * decided until binding to some counter in pmu->add().
	 *
	 * But since we don't have such support, later in pmu->add(), we just
	 * use hwc->config as the index instead.
	 */
	hwc->config_base = config_base;
	hwc->config = code;
	hwc->idx = -1;

	return 0;
}

/*
 * Initialization
 */

static struct pmu min_pmu = {
	.name		= "riscv-base",
	.event_init	= riscv_event_init,
	.add		= riscv_pmu_add,
	.del		= riscv_pmu_del,
	.start		= riscv_pmu_start,
	.stop		= riscv_pmu_stop,
	.read		= riscv_pmu_read,
};

static const struct riscv_pmu riscv_base_pmu = {
	.pmu = &min_pmu,
	.max_events = ARRAY_SIZE(riscv_hw_event_map),
	.map_hw_event = riscv_map_hw_event,
	.hw_events = riscv_hw_event_map,
	.map_cache_event = riscv_map_cache_event,
	.cache_events = &riscv_cache_event_map,
	.counter_width = 63,
	.num_counters = RISCV_BASE_COUNTERS + 0,
	.handle_irq = &riscv_base_pmu_handle_irq,

	/* This means this PMU has no IRQ. */
	.irq = -1,
};

static const struct of_device_id riscv_pmu_of_ids[] = {
	{.compatible = "riscv,base-pmu",	.data = &riscv_base_pmu},
	{ /* sentinel value */ }
};

static int __init init_hw_perf_events(void)
{
	struct device_node *node = of_find_node_by_type(NULL, "pmu");
	const struct of_device_id *of_id;

	riscv_pmu = &riscv_base_pmu;

	if (node) {
		of_id = of_match_node(riscv_pmu_of_ids, node);

		if (of_id)
			riscv_pmu = of_id->data;
		of_node_put(node);
	}

	perf_pmu_register(riscv_pmu->pmu, "cpu", PERF_TYPE_RAW);
	return 0;
}
arch_initcall(init_hw_perf_events);
