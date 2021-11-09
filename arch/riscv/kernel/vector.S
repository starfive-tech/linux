/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 * Copyright (C) 2019 Alibaba Group Holding Limited
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#include <linux/linkage.h>

#include <asm/asm.h>
#include <asm/csr.h>
#include <asm/asm-offsets.h>

#define vstatep  a0
#define datap    a1
#define x_vstart t0
#define x_vtype  t1
#define x_vl     t2
#define x_vcsr   t3
#define incr     t4
#define status   t5

ENTRY(__vstate_save)
	li      status, SR_VS
	csrs    CSR_STATUS, status

	csrr    x_vstart, CSR_VSTART
	csrr    x_vtype, CSR_VTYPE
	csrr    x_vl, CSR_VL
	csrr    x_vcsr, CSR_VCSR
	vsetvli incr, x0, e8, m8
	vse8.v   v0, (datap)
	add     datap, datap, incr
	vse8.v   v8, (datap)
	add     datap, datap, incr
	vse8.v   v16, (datap)
	add     datap, datap, incr
	vse8.v   v24, (datap)

	REG_S   x_vstart, RISCV_V_STATE_VSTART(vstatep)
	REG_S   x_vtype, RISCV_V_STATE_VTYPE(vstatep)
	REG_S   x_vl, RISCV_V_STATE_VL(vstatep)
	REG_S   x_vcsr, RISCV_V_STATE_VCSR(vstatep)

	csrc	CSR_STATUS, status
	ret
ENDPROC(__vstate_save)

ENTRY(__vstate_restore)
	li      status, SR_VS
	csrs    CSR_STATUS, status

	vsetvli incr, x0, e8, m8
	vle8.v   v0, (datap)
	add     datap, datap, incr
	vle8.v   v8, (datap)
	add     datap, datap, incr
	vle8.v   v16, (datap)
	add     datap, datap, incr
	vle8.v   v24, (datap)

	REG_L   x_vstart, RISCV_V_STATE_VSTART(vstatep)
	REG_L   x_vtype, RISCV_V_STATE_VTYPE(vstatep)
	REG_L   x_vl, RISCV_V_STATE_VL(vstatep)
	REG_L   x_vcsr, RISCV_V_STATE_VCSR(vstatep)
	vsetvl  x0, x_vl, x_vtype
	csrw    CSR_VSTART, x_vstart
	csrw    CSR_VCSR, x_vcsr

	csrc	CSR_STATUS, status
	ret
ENDPROC(__vstate_restore)

ENTRY(vector_flush_cpu_state)
	vsetvli t0, x0, e8, m8
	vmv.v.i v0, 0
	vmv.v.i v8, 0
	vmv.v.i v16, 0
	vmv.v.i v24, 0
	ret
ENDPROC(vector_flush_cpu_state)
