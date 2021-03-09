/*
 ******************************************************************************
 * @file  vic-pka.c
 * @author  StarFive Technology
 * @version  V1.0
 * @date  08/13/2020
 * @brief
 ******************************************************************************
 * @copy
 *
 * THE PRESENT SOFTWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME. AS A RESULT, STARFIVE SHALL NOT BE HELD LIABLE FOR ANY
 * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
 * FROM THE CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
 * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * <h2><center>&copy; COPYRIGHT 2020 Shanghai StarFive Technology Co., Ltd. </center></h2>
 */
#include <linux/mod_devicetable.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/crypto.h>
#include <linux/io.h>

#include <crypto/scatterwalk.h>

#include "vic-sec.h"
#include "vic-pka-hw.h"

#define ERROR(fmt, ...)  printk("ERROR %s() ln %d:" fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

static int pka_wait(struct vic_sec_dev *sdev)
{
	struct pka_state *pka = &sdev->pka;
	int ret = -1;
	mutex_lock(&sdev->doing);
	if(pka->pka_done || pka->pka_err)
		ret = 0;
	mutex_unlock(&sdev->doing);
	return ret;
	//return wait_cond_timeout(pka->pka_done || pka->pka_err, 10, 40000000);
}

#define PKA_LOAD(pka, bank, index, size, para)				\
	if (para) {							\
		rc = elppka_load_operand(pka, bank, index, size, para); \
		if (rc) {						\
			ERROR("failed to load a param\r\n");		\
			return -EIO;					\
		}							\
	}

#define PKA_UNLOAD(pka, bank, index, size, para)			\
	if (para) {							\
		rc = elppka_unload_operand(pka, bank, index, size, para); \
		if (rc) {						\
			ERROR("failed to unload a param\r\n");		\
			return -EIO;					\
		}							\
	}

#define PKA_RUN(sdev, pka, func, flags, size)				\
	do {								\
		mutex_lock(&sdev->doing);				\
		elppka_start(pka, func, flags, size);			\
		rc = pka_wait(sdev);					\
		if (rc) {						\
			ERROR("failed\r\n");				\
			return rc;					\
		}							\
	} while (0)



/**
   Base Modular Arithmetic Library Functions

   The Base Modular Arithmetic library suite provides a set of modular arithmetic operations commonly used by
   cryptographic applications. These include Montgomery precomputation operations and other generic modular
   operations.
**/

static int vic_rsa_calc_rinv(struct vic_sec_ctx *ctx, u32 size,
			     const u8 *m,
			     u8 *rinv) // C0
{
	struct vic_sec_dev *sdev = ctx->sdev;
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;

	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, m);
	PKA_RUN(sdev, pka, PKA_CALC_R_INV, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_C, 0, size, rinv);
	return rc;
}

static int vic_rsa_calc_mp(struct vic_sec_ctx *ctx, u32 size,
			   const u8 *rinv, // C0
			   const u8 *m, // D0
			   u8 *mp) // D1
{
	struct vic_sec_dev *sdev = ctx->sdev;
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_D, 2, size, rinv);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, m);
	PKA_RUN(sdev, pka, PKA_CALC_MP, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_D, 1, size, mp);
	return rc;
}

static int vic_rsa_calc_rsqr(struct vic_sec_ctx *ctx, u32 size,
			     const u8 *rinv, // C0
			     const u8 *m, // D0
			     const u8 *mp, // D1
			     u8 *rsqr) // D3
{
	struct vic_sec_dev *sdev = ctx->sdev;
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_C, 0, size, rinv);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, m);
	PKA_LOAD(pka, PKA_OPERAND_D, 1, size, mp);
	PKA_RUN(sdev, pka, PKA_CALC_R_SQR, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_D, 3, size, rsqr);
	return rc;
}


static int vic_rsa_calc_modexp(struct vic_sec_ctx *ctx, const u8 *src, // A0
			       u32 full_width, // F0
			       u8 *dst,
			       int enc) // A0
{
	struct vic_sec_dev *sdev = ctx->sdev;
	struct pka_state *pka = &ctx->sdev->pka;
	struct vic_rsa_key *rsa_key = &ctx->rsa_key;
	size_t size = rsa_key->key_sz;
	int rc;

	PKA_LOAD(pka, PKA_OPERAND_A, 0, size, src);
	if(enc) {
		PKA_LOAD(pka, PKA_OPERAND_D, 2, size, rsa_key->e);
	} else {
		PKA_LOAD(pka, PKA_OPERAND_D, 2, size, rsa_key->d);
	}
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, rsa_key->n);
	PKA_LOAD(pka, PKA_OPERAND_D, 1, size, rsa_key->mp);
	PKA_LOAD(pka, PKA_OPERAND_D, 3, size, rsa_key->rsqr);
	PKA_RUN(sdev, pka, PKA_MODEXP, (full_width&0x1)<<PKA_FLAG_F0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 0, size, dst);
	return rc;
}

static int vic_rsa_crt(struct vic_sec_ctx *ctx, u32 size,
		       const u8 *msg_lo, // A2
		       const u8 *msg_hi, // A3
		       u32 full_width, // F0
		       u8 *c_lo, // A0
		       u8 *c_hi) // A1
{
	struct vic_sec_dev *sdev = ctx->sdev;
	struct pka_state *pka = &ctx->sdev->pka;
	struct vic_rsa_key *rsa_key = &ctx->rsa_key;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 2, size, msg_lo);
	PKA_LOAD(pka, PKA_OPERAND_A, 3, size, msg_hi);
	PKA_LOAD(pka, PKA_OPERAND_B, 2, size, rsa_key->p);
	PKA_LOAD(pka, PKA_OPERAND_B, 3, size, rsa_key->q);
	PKA_LOAD(pka, PKA_OPERAND_C, 2, size, rsa_key->qinv);
	PKA_LOAD(pka, PKA_OPERAND_C, 3, size, rsa_key->dp);
	PKA_LOAD(pka, PKA_OPERAND_D, 2, size, rsa_key->dq);
	PKA_LOAD(pka, PKA_OPERAND_D, 5, size, rsa_key->rsqr_p);
	PKA_LOAD(pka, PKA_OPERAND_D, 4, size, rsa_key->pmp);
	PKA_LOAD(pka, PKA_OPERAND_D, 3, size, rsa_key->rsqr_q);
	PKA_LOAD(pka, PKA_OPERAND_D, 6, size, rsa_key->qmp);
	PKA_RUN(sdev, pka, PKA_CRT, (full_width&0x1)<<PKA_FLAG_F0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 0, size, c_lo);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 1, size, c_hi);
	return rc;
}
#if 0
static int pka_modmult(struct vic_sec_ctx *ctx, u32 size,
		       const u8 *x, // A0
		       const u8 *y, // B0
		       const u8 *m, // D0
		       const u8 *mp, // D1
		       const u8 *r_sqr, // D3
		       u8 *c) // A0
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 0, size, x);
	PKA_LOAD(pka, PKA_OPERAND_B, 0, size, y);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, m);
	PKA_LOAD(pka, PKA_OPERAND_D, 1, size, mp);
	PKA_LOAD(pka, PKA_OPERAND_D, 3, size, r_sqr);
	PKA_RUN(pka, PKA_MODMULT, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 0, size, c);
	return rc;
}

static int pka_modadd(struct vic_sec_ctx *ctx, u32 size,
		      const u8 *x, // A0
		      const u8 *y, // B0
		      const u8 *m, // D0
		      u8 *c) // A0
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 0, size, x);
	PKA_LOAD(pka, PKA_OPERAND_B, 0, size, y);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, m);
	PKA_RUN(pka, PKA_MODADD, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 0, size, c);
	return rc;
}

static int pka_modsub(struct vic_sec_ctx *ctx, u32 size,
		      const u8 *x, // A0
		      const u8 *y, // B0
		      const u8 *m, // D0
		      u8 *c) // A0
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 0, size, x);
	PKA_LOAD(pka, PKA_OPERAND_B, 0, size, y);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, m);
	PKA_RUN(pka, PKA_MODSUB, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 0, size, c);
	return rc;
}

static int pka_reduce(struct vic_sec_ctx *ctx, u32 size,
		      const u8 *x, // C0
		      const u8 *m, // D0
		      u8 *c) // A0
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_C, 0, size, x);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, m);
	PKA_RUN(pka, PKA_REDUCE, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 0, size, c);
	return rc;
}

static int pka_moddiv(struct vic_sec_ctx *ctx, u32 size,
		      const u8 *y, // C0
		      const u8 *x, // A0
		      const u8 *m, // D0
		      u8 *c) // C0
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_C, 0, size, y);
	PKA_LOAD(pka, PKA_OPERAND_A, 0, size, x);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, m);
	PKA_RUN(pka, PKA_MODDIV, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_C, 0, size, c);
	return rc;
}

static int pka_modinv(struct vic_sec_ctx *ctx, u32 size,
		      const u8 *x, // A0
		      const u8 *m, // D0
		      u8 *c) // C0
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 0, size, x);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, m);
	PKA_RUN(pka, PKA_MODINV, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_C, 0, size, c);
	return rc;
}

static int pka_mult(struct vic_sec_ctx *ctx, u32 size,
		    const u8 *a, // A0
		    const u8 *b, // B0
		    u8 *c_lo, // C0
		    u8 *c_hi) // C1
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 0, size, a);
	PKA_LOAD(pka, PKA_OPERAND_B, 0, size, b);
	PKA_RUN(pka, PKA_MULT, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_C, 0, size, c_lo);
	PKA_UNLOAD(pka, PKA_OPERAND_C, 1, size, c_hi);
	return rc;
}

static int vic_rsa_crt_key_setup(struct vic_sec_ctx *ctx) // A3
{
	struct pka_state *pka = &ctx->sdev->pka;
	struct vic_rsa_key *rsa_key = &ctx->rsa_key;
	size_t size = rsa_key->key_sz >> 1;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, rsa_key->p);
	PKA_LOAD(pka, PKA_OPERAND_B, 1, size, rsa_key->q);
	PKA_LOAD(pka, PKA_OPERAND_D, 1, size, rsa_key->d + size);
	PKA_LOAD(pka, PKA_OPERAND_D, 3, size, rsa_key->d);
	PKA_RUN(pka, PKA_CRT_KEY_SETUP, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_B, 1, size, rsa_key->dp);
	PKA_UNLOAD(pka, PKA_OPERAND_C, 0, size, rsa_key->dq);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 3, size, rsa_key->qinv);
	return rc;
}

static int pka_rsa_bit_serial_mod(struct vic_sec_ctx *ctx, u32 size,
				  const u8 *x, // C0
				  const u8 *m, // D0
				  u8 *c) // C0
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_C, 0, size, x);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, m);
	PKA_RUN(pka, PKA_BIT_SERIAL_MOD, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_C, 0, size, c);
	return rc;
}

static int pka_rsa_bit_serial_mod_dp(struct vic_sec_ctx *ctx, u32 size,
				     const u8 *x_lo, // C0
				     const u8 *x_hi, // C1
				     const u8 *m, // D0
				     u8 *c) // C0
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_C, 0, size, x_lo);
	PKA_LOAD(pka, PKA_OPERAND_C, 1, size, x_hi);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, m);
	PKA_RUN(pka, PKA_BIT_SERIAL_MOD_DP, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_C, 0, size, c);
	return rc;
}


static int pka_ecc_pmult(struct vic_sec_ctx *ctx, u32 size,
			 const u8 *px, // A2
			 const u8 *py, // B2
			 const u8 *a, // A6
			 const u8 *k, // D7
			 const u8 *w, // A7
			 const u8 *p, // D0
			 const u8 *pp, // D1
			 const u8 *r_sqr_p, // D3
			 u32 blinding, // F0
			 u32 is_a_m3, // F3
			 u8 *qx, // A2
			 u8 *qy) // B2
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	u32 flags;

	PKA_LOAD(pka, PKA_OPERAND_A, 2, size, px);
	PKA_LOAD(pka, PKA_OPERAND_B, 2, size, py);
	PKA_LOAD(pka, PKA_OPERAND_A, 6, size, a);
	PKA_LOAD(pka, PKA_OPERAND_D, 7, size, k);
	PKA_LOAD(pka, PKA_OPERAND_A, 7, size, w);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, p);
	PKA_LOAD(pka, PKA_OPERAND_D, 1, size, pp);
	PKA_LOAD(pka, PKA_OPERAND_D, 3, size, r_sqr_p);
	flags = ((blinding&0x1)<<PKA_FLAG_F0) |
		((is_a_m3&0x1)<<PKA_FLAG_F3);
	PKA_RUN(pka, PKA_PMULT, flags, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 2, size, qx);
	PKA_UNLOAD(pka, PKA_OPERAND_B, 2, size, qy);
	return rc;
}

static int pka_ecc_padd(struct vic_sec_ctx *ctx, u32 size,
			const u8 *px, // A2
			const u8 *py, // B2
			const u8 *qx, // A3
			const u8 *qy, // B3
			const u8 *a, // A6
			const u8 *p, // D0
			const u8 *pp, // D1
			const u8 *r_sqr_p, // D3
			u8 *rx, // A2
			u8 *ry) // B2
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 2, size, px);
	PKA_LOAD(pka, PKA_OPERAND_B, 2, size, py);
	PKA_LOAD(pka, PKA_OPERAND_A, 3, size, qx);
	PKA_LOAD(pka, PKA_OPERAND_B, 3, size, qy);
	PKA_LOAD(pka, PKA_OPERAND_A, 6, size, a);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, p);
	PKA_LOAD(pka, PKA_OPERAND_D, 1, size, pp);
	PKA_LOAD(pka, PKA_OPERAND_D, 3, size, r_sqr_p);
	PKA_RUN(pka, PKA_PADD, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 2, size, rx);
	PKA_UNLOAD(pka, PKA_OPERAND_B, 2, size, ry);
	return rc;
}

static int pka_ecc_pdbl(struct vic_sec_ctx *ctx, u32 size,
			const u8 *px, // A3
			const u8 *py, // B3
			const u8 *a, // A6
			const u8 *p, // D0
			const u8 *pp, // D1
			const u8 *r_sqr_p, // D3
			u8 *qx, // A2
			u8 *qy) // B2
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 3, size, px);
	PKA_LOAD(pka, PKA_OPERAND_B, 3, size, py);
	PKA_LOAD(pka, PKA_OPERAND_A, 6, size, a);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, p);
	PKA_LOAD(pka, PKA_OPERAND_D, 1, size, pp);
	PKA_LOAD(pka, PKA_OPERAND_D, 3, size, r_sqr_p);
	PKA_RUN(pka, PKA_PDBL, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 2, size, qx);
	PKA_UNLOAD(pka, PKA_OPERAND_B, 2, size, qy);
	return rc;
}

static int pka_ecc_pver(struct vic_sec_ctx *ctx, u32 size,
			const u8 *px, // A2
			const u8 *py, // B2
			const u8 *a, // A6
			const u8 *b, // A7
			const u8 *p, // D0
			const u8 *pp, // D1
			const u8 *r_sqr_p, // D3
			u32 *ok) // Z
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 2, size, px);
	PKA_LOAD(pka, PKA_OPERAND_B, 2, size, py);
	PKA_LOAD(pka, PKA_OPERAND_A, 6, size, a);
	PKA_LOAD(pka, PKA_OPERAND_A, 7, size, b);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, p);
	PKA_LOAD(pka, PKA_OPERAND_D, 1, size, pp);
	PKA_LOAD(pka, PKA_OPERAND_D, 3, size, r_sqr_p);
	PKA_RUN(pka, PKA_PVER, 0, size);
	if (ok) {
		u32 flags = vic_pka_io_read32((void *)&pka->regbase[PKA_FLAGS]);
		*ok = (flags>>PKA_FLAG_ZERO)&0x1;
	}
	return rc;
}

static int pka_ecc_shamir(struct vic_sec_ctx *ctx, u32 size,
			  const u8 *px, // A2
			  const u8 *py, // B2
			  const u8 *qx, // A3
			  const u8 *qy, // B3
			  const u8 *a, // A6
			  const u8 *k, // A7
			  const u8 *l, // D7
			  const u8 *p, // D0
			  const u8 *pp, // D1
			  const u8 *r_sqr_p, // D3
			  u32 is_a_m3, // F3
			  u8 *rx, // A2
			  u8 *ry) // B2
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 2, size, px);
	PKA_LOAD(pka, PKA_OPERAND_B, 2, size, py);
	PKA_LOAD(pka, PKA_OPERAND_A, 3, size, qx);
	PKA_LOAD(pka, PKA_OPERAND_B, 3, size, qy);
	PKA_LOAD(pka, PKA_OPERAND_A, 6, size, a);
	PKA_LOAD(pka, PKA_OPERAND_A, 7, size, k);
	PKA_LOAD(pka, PKA_OPERAND_D, 7, size, l);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, p);
	PKA_LOAD(pka, PKA_OPERAND_D, 1, size, pp);
	PKA_LOAD(pka, PKA_OPERAND_D, 3, size, r_sqr_p);
	PKA_RUN(pka, PKA_SHAMIR, (is_a_m3&0x1)<<PKA_FLAG_F3, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 2, size, rx);
	PKA_UNLOAD(pka, PKA_OPERAND_B, 2, size, ry);
	return rc;
}


static int pka_ecc_pmult_521(struct vic_sec_ctx *ctx, u32 size,
			     const u8 *px, // A2
			     const u8 *py, // B2
			     const u8 *a, // A6
			     const u8 *k, // D7
			     const u8 *w, // A7
			     const u8 *p, // D0
			     u32 blinding, // F0
			     u8 *qx, // A2
			     u8 *qy) // B2
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;

	PKA_LOAD(pka, PKA_OPERAND_A, 2, size, px);
	PKA_LOAD(pka, PKA_OPERAND_B, 2, size, py);
	PKA_LOAD(pka, PKA_OPERAND_A, 6, size, a);
	PKA_LOAD(pka, PKA_OPERAND_D, 7, size, k);
	PKA_LOAD(pka, PKA_OPERAND_A, 7, size, w);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, p);
	PKA_RUN(pka, PKA_PMULT_521, (blinding&0x1)<<PKA_FLAG_F0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 2, size, qx);
	PKA_UNLOAD(pka, PKA_OPERAND_B, 2, size, qy);
	return rc;
}

static int pka_ecc_padd_521(struct vic_sec_ctx *ctx, u32 size,
			    const u8 *px, // A2
			    const u8 *py, // B2
			    const u8 *qx, // A3
			    const u8 *qy, // B3
			    const u8 *p, // D0
			    u8 *rx, // A2
			    u8 *ry) // B2
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 2, size, px);
	PKA_LOAD(pka, PKA_OPERAND_B, 2, size, py);
	PKA_LOAD(pka, PKA_OPERAND_A, 3, size, qx);
	PKA_LOAD(pka, PKA_OPERAND_B, 3, size, qy);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, p);
	PKA_RUN(pka, PKA_PADD_521, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 2, size, rx);
	PKA_UNLOAD(pka, PKA_OPERAND_B, 2, size, ry);
	return rc;
}

static int pka_ecc_pdbl_521(struct vic_sec_ctx *ctx, u32 size,
			    const u8 *px, // A3
			    const u8 *py, // B3
			    const u8 *a, // A6
			    const u8 *p, // D0
			    const u8 *pp, // D1
			    const u8 *r_sqr_p, // D3
			    u8 *qx, // A2
			    u8 *qy) // B2
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 3, size, px);
	PKA_LOAD(pka, PKA_OPERAND_B, 3, size, py);
	PKA_LOAD(pka, PKA_OPERAND_A, 6, size, a);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, p);
	PKA_LOAD(pka, PKA_OPERAND_D, 1, size, pp);
	PKA_LOAD(pka, PKA_OPERAND_D, 3, size, r_sqr_p);
	PKA_RUN(pka, PKA_PDBL_521, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 2, size, qx);
	PKA_UNLOAD(pka, PKA_OPERAND_B, 2, size, qy);
	return rc;
}

static int pka_ecc_pver_521(struct vic_sec_ctx *ctx, u32 size,
			    const u8 *px, // A2
			    const u8 *py, // B2
			    const u8 *a, // A6
			    const u8 *b, // A7
			    const u8 *p, // D0
			    u32 *ok) // Z
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 2, size, px);
	PKA_LOAD(pka, PKA_OPERAND_B, 2, size, py);
	PKA_LOAD(pka, PKA_OPERAND_A, 6, size, a);
	PKA_LOAD(pka, PKA_OPERAND_A, 7, size, b);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, p);
	PKA_RUN(pka, PKA_PVER_521, 0, size);
	if (ok) {
		u32 flags = vic_pka_io_read32((void *)&pka->regbase[PKA_FLAGS]);
		*ok = (flags>>PKA_FLAG_ZERO)&0x1;
	}
	return rc;
}

static int pka_ecc_modmult_521(struct vic_sec_ctx *ctx, u32 size,
			       const u8 *x, // A0
			       const u8 *y, // B0
			       const u8 *m, // D0
			       u8 *c) // A0
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 0, size, x);
	PKA_LOAD(pka, PKA_OPERAND_B, 0, size, y);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, m);
	PKA_RUN(pka, PKA_MODMULT_521, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 0, size, c);
	return rc;
}

static int pka_ecc_m_521_montmult(struct vic_sec_ctx *ctx, u32 size,
				  const u8 *x, // A0
				  const u8 *y, // B0
				  const u8 *m, // D0
				  const u8 *mp, // D1
				  const u8 *r_sqr, // D3
				  u8 *c) // A0
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 0, size, x);
	PKA_LOAD(pka, PKA_OPERAND_B, 0, size, y);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, m);
	PKA_LOAD(pka, PKA_OPERAND_D, 1, size, mp);
	PKA_LOAD(pka, PKA_OPERAND_D, 3, size, r_sqr);
	PKA_RUN(pka, PKA_M_521_MONTMULT, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 0, size, c);
	return rc;
}

static int pka_ecc_shamir_521(struct vic_sec_ctx *ctx, u32 size,
			      const u8 *px, // A2
			      const u8 *py, // B2
			      const u8 *qx, // A3
			      const u8 *qy, // B3
			      const u8 *a, // A6
			      const u8 *k, // A7
			      const u8 *l, // D7
			      const u8 *p, // D0
			      u8 *rx, // A2
			      u8 *ry) // B2
{
	struct pka_state *pka = &ctx->sdev->pka;
	int rc;
	PKA_LOAD(pka, PKA_OPERAND_A, 2, size, px);
	PKA_LOAD(pka, PKA_OPERAND_B, 2, size, py);
	PKA_LOAD(pka, PKA_OPERAND_A, 3, size, qx);
	PKA_LOAD(pka, PKA_OPERAND_B, 3, size, qy);
	PKA_LOAD(pka, PKA_OPERAND_A, 6, size, a);
	PKA_LOAD(pka, PKA_OPERAND_A, 7, size, k);
	PKA_LOAD(pka, PKA_OPERAND_D, 7, size, l);
	PKA_LOAD(pka, PKA_OPERAND_D, 0, size, p);
	PKA_RUN(pka, PKA_SHAMIR_521, 0, size);
	PKA_UNLOAD(pka, PKA_OPERAND_A, 2, size, rx);
	PKA_UNLOAD(pka, PKA_OPERAND_B, 2, size, ry);
	return rc;
}

static void pka_clear_state(struct vic_sec_ctx *ctx)
{
	struct pka_state *pka = &ctx->sdev->pka;
	pka->pka_done = 0;
	pka->pka_err = 0;
}
#endif
static void vic_rsa_free_key(struct vic_rsa_key *key)
{
	if(key->d)
		kfree_sensitive(key->d);
	if(key->p)
		kfree_sensitive(key->p);
	if(key->q)
		kfree_sensitive(key->q);
	if(key->dp)
		kfree_sensitive(key->dp);
	if(key->dq)
		kfree_sensitive(key->dq);
	if(key->qinv)
		kfree_sensitive(key->qinv);
	if(key->rinv)
		kfree_sensitive(key->rinv);
	if(key->rinv_p)
		kfree_sensitive(key->rinv_p);
	if(key->rinv_q)
		kfree_sensitive(key->rinv_q);
	if(key->mp)
		kfree_sensitive(key->mp);
	if(key->rsqr)
		kfree_sensitive(key->rsqr);
	if(key->rsqr_p)
		kfree_sensitive(key->rsqr_p);
	if(key->rsqr_q)
		kfree_sensitive(key->rsqr_q);
	if(key->pmp)
		kfree_sensitive(key->pmp);
	if(key->qmp)
		kfree_sensitive(key->qmp);
	if(key->e)
		kfree(key->e);
	if(key->n)
		kfree(key->n);
	memset(key, 0, sizeof(*key));
}

static int vic_rsa_pre_cal(struct vic_sec_ctx *ctx)
{
	struct vic_rsa_key *rsa_key = &ctx->rsa_key;
	size_t size = rsa_key->key_sz;
	int ret = -ENOMEM;

	rsa_key->rinv = kzalloc(size, GFP_KERNEL);
	if(!rsa_key->rinv)
		goto err;

	rsa_key->mp = kzalloc(size, GFP_KERNEL);
	if(!rsa_key->mp)
		goto err;

	rsa_key->rsqr = kzalloc(size, GFP_KERNEL);
	if(!rsa_key->rsqr)
		goto err;

	ret = vic_rsa_calc_rinv(ctx, size, rsa_key->n, rsa_key->rinv);
	if(ret)
		return ret;
	ret = vic_rsa_calc_mp(ctx, size, rsa_key->rinv,rsa_key->n,rsa_key->mp);
	if(ret)
		return ret;

	ret = vic_rsa_calc_rsqr(ctx, size, rsa_key->rinv, rsa_key->n, rsa_key->mp,
				rsa_key->rsqr);

 err:
	return ret;
}

static int vic_rsa_pre_cal_crt(struct vic_sec_ctx *ctx)
{
	struct vic_rsa_key *rsa_key = &ctx->rsa_key;
	int ret = -ENOMEM;
	size_t size = rsa_key->key_sz >> 1;

	rsa_key->rinv_p = kzalloc(size, GFP_KERNEL);
	if(!rsa_key->rinv_p)
		goto err;

	rsa_key->rinv_q = kzalloc(size, GFP_KERNEL);
	if(!rsa_key->rinv_q)
		goto err;

	rsa_key->pmp = kzalloc(size, GFP_KERNEL);
	if(!rsa_key->pmp)
		goto err;

	rsa_key->qmp = kzalloc(size, GFP_KERNEL);
	if(!rsa_key->qmp)
		goto err;

	rsa_key->rsqr_p = kzalloc(size, GFP_KERNEL);
	if(!rsa_key->rsqr_p)
		goto err;

	rsa_key->rsqr_q = kzalloc(size, GFP_KERNEL);
	if(!rsa_key->rsqr_q)
		goto err;

	ret = vic_rsa_calc_rinv(ctx, size, rsa_key->p, rsa_key->rinv_p);
	if(ret)
		return ret;

	ret = vic_rsa_calc_mp(ctx, size, rsa_key->rinv_p, rsa_key->p, rsa_key->pmp);
	if(ret)
		return ret;

	ret = vic_rsa_calc_rsqr(ctx, size, rsa_key->rinv_p, rsa_key->p, rsa_key->pmp,
				rsa_key->rsqr_p);
	if(ret)
		return ret;


	ret = vic_rsa_calc_rinv(ctx, size, rsa_key->q, rsa_key->rinv_q);
	if(ret)
		return ret;

	ret = vic_rsa_calc_mp(ctx, size, rsa_key->rinv_q, rsa_key->q, rsa_key->qmp);
	if(ret)
		return ret;

	ret = vic_rsa_calc_rsqr(ctx, size, rsa_key->rinv_q, rsa_key->q, rsa_key->qmp,
				rsa_key->rsqr_q);

 err:
	return ret;
}

static int vic_rsa_enc_core(struct vic_sec_ctx *ctx, int enc)
{
	struct vic_sec_dev *sdev = ctx->sdev;
	struct vic_sec_request_ctx *rctx = sdev->rctx;
	struct vic_rsa_key *key = &ctx->rsa_key;
	size_t data_len, total, count, data_offset;
	int ret = 0;

	rctx->offset = 0;
	total = 0;


	while(total < sdev->total_in) {
		count = min (sdev->data_buf_len, sdev->total_in);
		count = min (count, key->key_sz);
		memset(sdev->data, 0, key->key_sz);
		data_offset = key->key_sz - count;
		data_len = vic_cryp_get_from_sg(rctx, rctx->offset, count, data_offset);
		if(data_len < 0)
			return data_len;
		if(data_len != count) {
			return -EINVAL;
		}
		if(!enc && key->crt_mode) {
			size_t size = key->key_sz >> 1;
			ret = vic_rsa_crt(ctx, size, sdev->data + size, sdev->data,
					  0,sdev->data + key->key_sz + size, sdev->data + key->key_sz);
		} else {
			ret = vic_rsa_calc_modexp(ctx, sdev->data, 0, sdev->data + key->key_sz, enc);
		}
		if(ret) {
			return ret;
		}

		sg_copy_buffer(rctx->out_sg,sg_nents(rctx->out_sg), sdev->data + key->key_sz,
			       key->key_sz, rctx->offset, 0);

		rctx->offset += data_len;
		total += data_len;
	}

	return ret;
}

static int vic_rsa_enc(struct akcipher_request *req)
{
	struct crypto_akcipher *tfm = crypto_akcipher_reqtfm(req);
	struct vic_sec_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct vic_rsa_key *key = &ctx->rsa_key;
	struct vic_sec_request_ctx *rctx = akcipher_request_ctx(req);
	int ret = 0;

	if (unlikely(!key->n || !key->e))
		return -EINVAL;


	if (req->dst_len < key->key_sz) {
		req->dst_len = key->key_sz;
		dev_err(ctx->sdev->dev, "Output buffer length less than parameter n\n");
		return -EOVERFLOW;
	}

	rctx->sg = req->src;
	rctx->out_sg = req->dst;
	rctx->sdev = ctx->sdev;
	ctx->sdev->rctx = rctx;
	ctx->sdev->total_in = req->src_len;
	ctx->sdev->total_out = req->dst_len;

	ret = vic_rsa_pre_cal(ctx);
	if(ret) {
		return ret;
	}

	ret = vic_rsa_enc_core(ctx, 1);

	return ret;
}

static int vic_rsa_dec(struct akcipher_request *req)
{
	struct crypto_akcipher *tfm = crypto_akcipher_reqtfm(req);
	struct vic_sec_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct vic_rsa_key *key = &ctx->rsa_key;
	struct vic_sec_request_ctx *rctx = akcipher_request_ctx(req);
	int ret = 0;

	if (unlikely(!key->n || !key->d))
		return -EINVAL;

	if (req->dst_len < key->key_sz) {
		req->dst_len = key->key_sz;
		dev_err(ctx->sdev->dev, "Output buffer length less than parameter n\n");
		return -EOVERFLOW;
	}

	rctx->sg = req->src;
	rctx->out_sg = req->dst;
	rctx->sdev = ctx->sdev;
	ctx->sdev->rctx = rctx;
	ctx->sdev->total_in = req->src_len;
	ctx->sdev->total_out = req->dst_len;

	if(key->crt_mode) {
		ret = vic_rsa_pre_cal_crt(ctx);
	} else {
		ret = vic_rsa_pre_cal(ctx);
	}

	if(ret) {
		return ret;
	}

	ret = vic_rsa_enc_core(ctx, 0);
	return ret;
}

static unsigned long vic_rsa_enc_fn_id(unsigned int len)
{
	unsigned int bitslen = len << 3;

	switch (bitslen) {
		//case 256:
	case 512:
	case 768:
	case 1024:
	case 1536:
	case 2048:
	case 3072:
	case 4096:
		return 0;
	default:
		return -EINVAL;
	};
}

static int vic_rsa_set_n(struct vic_rsa_key *rsa_key, const char *value,
			 size_t vlen)
{
	const char *ptr = value;
	int ret;

	while (!*ptr && vlen) {
		ptr++;
		vlen--;
	}

	rsa_key->key_sz = vlen;
	ret = -EINVAL;
	/* invalid key size provided */
	if (vic_rsa_enc_fn_id(rsa_key->key_sz))
		goto err;

	ret = -ENOMEM;
	rsa_key->n = kmemdup(ptr, rsa_key->key_sz, GFP_KERNEL);
	if (!rsa_key->n)
		goto err;

	return 0;
 err:
	rsa_key->key_sz = 0;
	rsa_key->n = NULL;
	return ret;
}

static int vic_rsa_set_e(struct vic_rsa_key *rsa_key, const char *value,
			 size_t vlen)
{
	const char *ptr = value;

	while (!*ptr && vlen) {
		ptr++;
		vlen--;
	}

	if (!rsa_key->key_sz || !vlen || vlen > rsa_key->key_sz) {
		rsa_key->e = NULL;
		return -EINVAL;
	}

	rsa_key->e = kzalloc(rsa_key->key_sz, GFP_KERNEL);
	if (!rsa_key->e)
		return -ENOMEM;

	memcpy(rsa_key->e + (rsa_key->key_sz - vlen), ptr, vlen);
	return 0;
}

static int vic_rsa_set_d(struct vic_rsa_key *rsa_key, const char *value,
			 size_t vlen)
{
	const char *ptr = value;
	int ret;

	while (!*ptr && vlen) {
		ptr++;
		vlen--;
	}

	ret = -EINVAL;
	if (!rsa_key->key_sz || !vlen || vlen > rsa_key->key_sz)
		goto err;

	ret = -ENOMEM;
	rsa_key->d = kzalloc(rsa_key->key_sz, GFP_KERNEL);
	if (!rsa_key->d)
		goto err;

	memcpy(rsa_key->d + (rsa_key->key_sz - vlen), ptr, vlen);
	return 0;
 err:
	rsa_key->d = NULL;
	return ret;
}

static void vic_rsa_drop_leading_zeros(const char **ptr, unsigned int *len)
{
	while (!**ptr && *len) {
		(*ptr)++;
		(*len)--;
	}
}

static void vic_rsa_setkey_crt(struct vic_rsa_key *rsa_key, struct rsa_key *raw_key)
{
	const char *ptr;
	unsigned int len;
	unsigned int half_key_sz = rsa_key->key_sz / 2;

	/* p */
	ptr = raw_key->p;
	len = raw_key->p_sz;
	vic_rsa_drop_leading_zeros(&ptr, &len);
	if (!len) {
		goto err;
	}
	rsa_key->p = kzalloc(half_key_sz, GFP_KERNEL);
	if (!rsa_key->p) {
		goto err;
	}
	memcpy(rsa_key->p + (half_key_sz - len), ptr, len);

	/* q */
	ptr = raw_key->q;
	len = raw_key->q_sz;
	vic_rsa_drop_leading_zeros(&ptr, &len);
	if (!len) {
		goto free_p;
	}
	rsa_key->q = kzalloc(half_key_sz, GFP_KERNEL);
	if (!rsa_key->q) {
		goto free_p;
	}
	memcpy(rsa_key->q + (half_key_sz - len), ptr, len);

	/* dp */
	ptr = raw_key->dp;
	len = raw_key->dp_sz;
	vic_rsa_drop_leading_zeros(&ptr, &len);
	if (!len) {
		goto free_q;
	}
	rsa_key->dp = kzalloc(half_key_sz, GFP_KERNEL);
	if (!rsa_key->dp) {
		goto free_q;
	}
	memcpy(rsa_key->dp + (half_key_sz - len), ptr, len);

	/* dq */
	ptr = raw_key->dq;
	len = raw_key->dq_sz;
	vic_rsa_drop_leading_zeros(&ptr, &len);
	if (!len) {
		goto free_dp;
	}
	rsa_key->dq = kzalloc(half_key_sz, GFP_KERNEL);
	if (!rsa_key->dq) {
		goto free_dp;
	}
	memcpy(rsa_key->dq + (half_key_sz - len), ptr, len);

	/* qinv */
	ptr = raw_key->qinv;
	len = raw_key->qinv_sz;
	vic_rsa_drop_leading_zeros(&ptr, &len);
	if (!len) {
		goto free_dq;
	}
	rsa_key->qinv = kzalloc(half_key_sz, GFP_KERNEL);
	if (!rsa_key->qinv) {
		goto free_dq;
	}
	memcpy(rsa_key->qinv + (half_key_sz - len), ptr, len);

	rsa_key->crt_mode = true;
	return;

 free_dq:
	memset(rsa_key->dq, '\0', half_key_sz);
	kfree_sensitive(rsa_key->dq);
	rsa_key->dq = NULL;
 free_dp:
	memset(rsa_key->dp, '\0', half_key_sz);
	kfree_sensitive(rsa_key->dp);
	rsa_key->dp = NULL;
 free_q:
	memset(rsa_key->q, '\0', half_key_sz);
	kfree_sensitive(rsa_key->q);
	rsa_key->q = NULL;
 free_p:
	memset(rsa_key->p, '\0', half_key_sz);
	kfree_sensitive(rsa_key->p);
	rsa_key->p = NULL;
 err:
	rsa_key->crt_mode = false;
}

static int vic_rsa_setkey(struct crypto_akcipher *tfm, const void *key,
			  unsigned int keylen, bool private)
{
	struct vic_sec_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct rsa_key raw_key = {NULL};
	struct vic_rsa_key *rsa_key = &ctx->rsa_key;
	int ret;

	vic_rsa_free_key(rsa_key);

	if (private)
		ret = rsa_parse_priv_key(&raw_key, key, keylen);
	else
		ret = rsa_parse_pub_key(&raw_key, key, keylen);
	if (ret < 0)
		goto free;

	ret = vic_rsa_set_n(rsa_key, raw_key.n, raw_key.n_sz);
	if (ret < 0)
		goto free;
	ret = vic_rsa_set_e(rsa_key, raw_key.e, raw_key.e_sz);
	if (ret < 0)
		goto free;
	if (private) {
		ret = vic_rsa_set_d(rsa_key, raw_key.d, raw_key.d_sz);
		if (ret < 0)
			goto free;
		vic_rsa_setkey_crt(rsa_key, &raw_key);
	}

	if (!rsa_key->n || !rsa_key->e) {
		/* invalid key provided */
		ret = -EINVAL;
		goto free;
	}
	if (private && !rsa_key->d) {
		/* invalid private key provided */
		ret = -EINVAL;
		goto free;
	}

	return 0;
 free:
	vic_rsa_free_key(rsa_key);
	return ret;
}

static int vic_rsa_set_pub_key(struct crypto_akcipher *tfm, const void *key,
			       unsigned int keylen)
{
	return vic_rsa_setkey(tfm, key, keylen, false);
}

static int vic_rsa_set_priv_key(struct crypto_akcipher *tfm, const void *key,
				unsigned int keylen)
{
	return vic_rsa_setkey(tfm, key, keylen, true);
}

static unsigned int vic_rsa_max_size(struct crypto_akcipher *tfm)
{
	struct vic_sec_ctx *ctx = akcipher_tfm_ctx(tfm);

	return ctx->rsa_key.key_sz;
}

static int vic_pka_reload_firmware(struct pka_state *pka)
{
	u32 fw_words, i, *fw;
	int ret = 0;

	fw_words = sizeof(PKA_FW)/sizeof(u32);
	if (fw_words > pka->cfg.fw_ram_size) {
		ERROR("large firmware\r\n");
		return -EINVAL;
	}

	fw = (u32 *)(pka->regbase + pka->cfg.ram_offset);
	for (i = 0; i < fw_words; fw++, i++) {
		vic_pka_io_write32(fw, PKA_FW[i]);
	}

	return ret;
}

/* Per session pkc's driver context creation function */
static int vic_rsa_init_tfm(struct crypto_akcipher *tfm)
{
	struct vic_sec_ctx *ctx = akcipher_tfm_ctx(tfm);

	//akcipher_set_reqsize(tfm, sizeof(struct vic_sec_request_ctx));
	ctx->sdev = vic_sec_find_dev(ctx);

	if (!ctx->sdev)
		return -ENODEV;
	mutex_lock(&ctx->sdev->lock);
	vic_clk_enable(ctx->sdev,PKA_CLK);
	vic_pka_reload_firmware(&ctx->sdev->pka);

	return 0;
}

/* Per session pkc's driver context cleanup function */
static void vic_rsa_exit_tfm(struct crypto_akcipher *tfm)
{
	struct vic_sec_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct vic_rsa_key *key = (struct vic_rsa_key *)&ctx->rsa_key;

	vic_rsa_free_key(key);
	vic_clk_disable(ctx->sdev,PKA_CLK);
	mutex_unlock(&ctx->sdev->lock);
	//vic_jr_free(ctx->dev);
}

irqreturn_t vic_pka_irq_done(struct vic_sec_dev *sdev)
{
	struct pka_state *pka = &sdev->pka;
   	u32 status;

   	status = vic_pka_io_read32((void *)&sdev->pka.regbase[PKA_F_STACK]);
   	if (status & 0xF) {
		pka->pka_err |= BIT(PKA_F_STACK);
   	}

   	status = vic_pka_io_read32((void *)&sdev->pka.regbase[PKA_STATUS]);
   	if (!(status & BIT(PKA_STAT_IRQ))) {
		pka->pka_err |= BIT(PKA_STAT_IRQ);
   	}

   	status = vic_pka_io_read32((void *)&sdev->pka.regbase[PKA_RC]);
   	if (status & 0x00FF0000) {
		pka->pka_err |= BIT(PKA_RC);
   	}

   	vic_pka_io_write32(&sdev->pka.regbase[PKA_STATUS], BIT(PKA_STAT_IRQ));

   	status = vic_pka_io_read32((void *)&sdev->pka.regbase[PKA_STATUS]);
   	if (status & BIT(PKA_STAT_IRQ)) {
		pka->pka_err |= BIT(PKA_STAT_IRQ);
   	}

	pka->pka_done = 1;
	status = vic_pka_io_read32((void *)&sdev->pka.regbase[PKA_IRQ_EN]);

   	//up(&sdev->core_running);
   	return IRQ_WAKE_THREAD;
}

static struct akcipher_alg vic_rsa = {
	.encrypt = vic_rsa_enc,
	.decrypt = vic_rsa_dec,
	.sign = vic_rsa_dec,
	.verify = vic_rsa_enc,
	.set_pub_key = vic_rsa_set_pub_key,
	.set_priv_key = vic_rsa_set_priv_key,
	.max_size = vic_rsa_max_size,
	.init = vic_rsa_init_tfm,
	.exit = vic_rsa_exit_tfm,
	.reqsize = sizeof(struct vic_sec_request_ctx),
	.base = {
		.cra_name = "rsa",
		.cra_driver_name = "rsa-vic",
		.cra_flags = CRYPTO_ALG_TYPE_AKCIPHER |
		            CRYPTO_ALG_ASYNC,
		.cra_priority = 3000,
		.cra_module = THIS_MODULE,
		.cra_ctxsize = sizeof(struct vic_sec_ctx),
	},
};

int vic_pka_init(struct pka_state *pka)
{
	u32 fw_words, i, *fw;
	int ret = 0;

	ret = elppka_setup(pka);
	if(ret)
		return ret;

	fw_words = sizeof(PKA_FW)/sizeof(u32);
	if (fw_words > pka->cfg.fw_ram_size) {
		ERROR("large firmware\r\n");
		return -EINVAL;
	}

	fw = (u32 *)(pka->regbase + pka->cfg.ram_offset);
	for (i = 0; i < fw_words; fw++, i++) {
		vic_pka_io_write32(fw, PKA_FW[i]);
	}

	vic_pka_io_write32(&pka->regbase[PKA_IRQ_EN], 1 << PKA_IRQ_EN_STAT);

 	vic_pka_io_write32(&pka->regbase[PKA_WATCHDOG], 100000000);

	return ret;
}

int vic_pka_register_algs(void)
{
	int ret;

	ret = crypto_register_akcipher(&vic_rsa);
	if (ret)
		printk("VIC RSA registration failed\n");

	return ret;
}

int vic_pka_unregister_algs(void)
{
	crypto_unregister_akcipher(&vic_rsa);
	return 0;
}
