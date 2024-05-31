// SPDX-License-Identifier: GPL-2.0-only
/*
 * Multiplex several IPIs over a single HW IPI.
 *
 * Copyright (c) 2022 Ventana Micro Systems Inc.
 */

#define pr_fmt(fmt) "riscv: " fmt
#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <asm/sbi.h>

static int sbi_ipi_virq;

#ifdef CONFIG_RISCV_AMP
static struct amp_data riscv_amp_data[NR_CPUS] __cacheline_aligned;

static unsigned long riscv_get_extra_bits(void)
{
	int cpu_id;
	unsigned long bits = 0;

	cpu_id = smp_processor_id();
	if (riscv_amp_data[cpuid_to_hartid_map(cpu_id)].amp_bits)
		bits |= BIT(IPI_AMP);

	return bits;
}

unsigned long riscv_clear_amp_bits(void)
{
	int cpu_id;
	unsigned long *ops;

	/*atomic ops */
	cpu_id = smp_processor_id();
	ops = &riscv_amp_data[cpuid_to_hartid_map(cpu_id)].amp_bits;
	return xchg(ops, 0);
}
#endif

static void sbi_ipi_handle(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);

	chained_irq_enter(chip, desc);

	csr_clear(CSR_IP, IE_SIE);
	ipi_mux_process();

	chained_irq_exit(chip, desc);
}

static int sbi_ipi_starting_cpu(unsigned int cpu)
{
	enable_percpu_irq(sbi_ipi_virq, irq_get_trigger_type(sbi_ipi_virq));
	return 0;
}

void __init sbi_ipi_init(void)
{
	int virq, irq_num;
	struct irq_domain *domain;
	struct fwnode_handle *node;

	if (riscv_ipi_have_virq_range())
		return;

	node = riscv_get_intc_hwnode();
	domain = irq_find_matching_fwnode(node,
					  DOMAIN_BUS_ANY);
	if (!domain) {
		pr_err("unable to find INTC IRQ domain\n");
		return;
	}

	sbi_ipi_virq = irq_create_mapping(domain, RV_IRQ_SOFT);
	if (!sbi_ipi_virq) {
		pr_err("unable to create INTC IRQ mapping\n");
		return;
	}
#ifdef CONFIG_RISCV_AMP
	if (fwnode_property_present(node, "enable-ipi-amp")) {
		riscv_set_ipi_amp_enable();
		sbi_amp_data_init(riscv_amp_data);
		ipi_set_extra_bits(riscv_get_extra_bits);
		irq_num = BITS_PER_TYPE(short);
	} else
		irq_num = BITS_PER_BYTE;
#else
	irq_num = BITS_PER_BYTE;
#endif

	virq = ipi_mux_create(irq_num, sbi_send_ipi);
	if (virq <= 0) {
		pr_err("unable to create muxed IPIs\n");
		irq_dispose_mapping(sbi_ipi_virq);
		return;
	}

	irq_set_chained_handler(sbi_ipi_virq, sbi_ipi_handle);

	/*
	 * Don't disable IPI when CPU goes offline because
	 * the masking/unmasking of virtual IPIs is done
	 * via generic IPI-Mux
	 */
	cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
			  "irqchip/sbi-ipi:starting",
			  sbi_ipi_starting_cpu, NULL);

	riscv_ipi_set_virq_range(virq, irq_num, false);
	pr_info("providing IPIs using SBI IPI extension\n");
}
