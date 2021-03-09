/*
 ******************************************************************************
 * @file  vic-sec.c
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

#include <crypto/scatterwalk.h>

#include "vic-sec.h"
#include "vic-pka.h"
#include "vic-pka-hw.h"

#define MAX(a, b) (((a)>(b)) ? (a) : (b))

/*
 * Determine the base radix for the given operand size,
 *   ceiling(lg(size/8))
 * where size > 16 bytes.
 * Returns 0 if the size is invalid.
 */
static unsigned elppka_base_radix(unsigned size)
{
	if (size <= 16)  return 0; /* Error */
	if (size <= 32)  return 2;
	if (size <= 64)  return 3;
	if (size <= 128) return 4;
	if (size <= 256) return 5;
	if (size <= 512) return 6;

	return 0;
}

/*
 * Helper to compute the operand page size, which depends only on the base
 * radix.
 */
static unsigned elppka_page_size(unsigned size)
{
	unsigned ret;

	ret = elppka_base_radix(size);
	if (!ret)
		return ret;
	return 8 << ret;
}

/*
 * Check that the given PKA operand index is valid for a particular bank and
 * operand size.  The bank and size values themselves are not validated.
 */
static int index_is_valid(const struct pka_config *cfg, unsigned bank,
			  unsigned index, unsigned size)
{
	unsigned ecc_max_bytes, rsa_max_bytes, abc_storage, d_storage;

	ecc_max_bytes = cfg->ecc_size >> 3;
	rsa_max_bytes = cfg->rsa_size >> 3;

	if (size > ecc_max_bytes && size > rsa_max_bytes)
		return 0;
	if (index > 7)
		return 0;

	abc_storage = MAX(ecc_max_bytes*8, rsa_max_bytes*2);
	d_storage   = MAX(ecc_max_bytes*8, rsa_max_bytes*4);

	if (bank == PKA_OPERAND_D) {
		return index < d_storage / size;
	} else {
		return index < abc_storage / size;
	}
}

/*
 * Determine the offset (in 32-bit words) of a particular operand in the PKA
 * memory map.
 * Returns the (non-negative) offset on success, or -errno on failure.
 */
static int operand_base_offset(const struct pka_config *cfg, unsigned bank,
			       unsigned index, unsigned size)
{
	unsigned pagesize;
	int ret;

	pagesize = elppka_page_size(size);
	if (!pagesize)
		return CRYPTO_INVALID_SIZE;

	if (!index_is_valid(cfg, bank, index, pagesize))
		return CRYPTO_NOT_FOUND;

	switch (bank) {
	case PKA_OPERAND_A:
		ret = PKA_OPERAND_A_BASE;
		break;
	case PKA_OPERAND_B:
		ret = PKA_OPERAND_B_BASE;
		break;
	case PKA_OPERAND_C:
		ret = PKA_OPERAND_C_BASE;
		break;
	case PKA_OPERAND_D:
		ret = PKA_OPERAND_D_BASE;
		break;
	default:
		return CRYPTO_INVALID_ARGUMENT;
	}

	return ret + index * (pagesize>>2);
}

/* Parse out the fields from a type-0 BUILD_CONF register in bc. */
static void elppka_get_config_type0(uint32_t bc, struct pka_config *out)
{
	struct pka_config cfg = {0};

	if (bc & (1ul << PKA_BC_FW_HAS_RAM)) {
		cfg.fw_ram_size = 256u << ((bc >> PKA_BC_FW_RAM_SZ)
					   & ((1ul << PKA_BC_FW_RAM_SZ_BITS)-1));
	}
	if (bc & (1ul << PKA_BC_FW_HAS_ROM)) {
		cfg.fw_rom_size = 256u << ((bc >> PKA_BC_FW_ROM_SZ)
					   & ((1ul << PKA_BC_FW_ROM_SZ_BITS)-1));
	}

	cfg.alu_size = 32u << ((bc >> PKA_BC_ALU_SZ)
			       & ((1ul << PKA_BC_ALU_SZ_BITS)-1));
	cfg.rsa_size = 512u << ((bc >> PKA_BC_RSA_SZ)
				& ((1ul << PKA_BC_RSA_SZ_BITS)-1));
	cfg.ecc_size = 256u << ((bc >> PKA_BC_ECC_SZ)
				& ((1ul << PKA_BC_ECC_SZ_BITS)-1));

	*out = cfg;
}

/* Parse out the fields from a type-1 BUILD_CONF register in bc. */
static void elppka_get_config_type1(uint32_t bc, struct pka_config *out)
{
	struct pka_config cfg = {0};
	uint32_t tmp;

	tmp = (bc >> PKA_BC1_FW_RAM_SZ) & ((1ul << PKA_BC1_FW_RAM_SZ_BITS)-1);
	if (tmp)
		cfg.fw_ram_size = 256u << (tmp-1);

	tmp = (bc >> PKA_BC1_FW_ROM_SZ) & ((1ul << PKA_BC1_FW_ROM_SZ_BITS)-1);
	if (tmp)
		cfg.fw_rom_size = 256u << (tmp-1);

	tmp = (bc >> PKA_BC1_RSA_SZ) & ((1ul << PKA_BC1_RSA_SZ_BITS)-1);
	if (tmp)
		cfg.rsa_size = 512u << (tmp-1);

	tmp = (bc >> PKA_BC1_ECC_SZ) & ((1ul << PKA_BC1_ECC_SZ_BITS)-1);
	if (tmp)
		cfg.ecc_size = 256u << (tmp-1);

	tmp = (bc >> PKA_BC1_ALU_SZ) & ((1ul << PKA_BC1_ALU_SZ_BITS)-1);
	cfg.alu_size = 32u << tmp;

	*out = cfg;
}

/* Read out PKA H/W configuration into config structure. */
static int elppka_get_config(uint32_t *regs, struct pka_config *out)
{
	uint32_t bc = vic_pka_io_read32(&regs[PKA_BUILD_CONF]);

	unsigned type = bc >> PKA_BC_FORMAT_TYPE;
	type &= (1ul << PKA_BC_FORMAT_TYPE_BITS) - 1;

	switch (type) {
	case 0:
		elppka_get_config_type0(bc, out);
		break;
	case 1:
	case 2: /* Type 2 has same format as type 1 */
		elppka_get_config_type1(bc, out);
		break;
	}

	/* RAM/ROM base addresses depend on core version */
	if (type < 2) {
		out->ram_offset = PKA_FIRMWARE_BASE;
		out->rom_offset = PKA_FIRMWARE_BASE + out->fw_ram_size;
	} else {
		out->ram_offset = out->rom_offset = PKA_FIRMWARE_T2_BASE;
		if (out->fw_ram_size)
			out->rom_offset = PKA_FIRMWARE_T2_SPLIT;
	}

	return 0;
}

int elppka_start(struct pka_state *pka, uint32_t entry, uint32_t flags,
		 unsigned size)
{
	uint32_t ctrl, base;

	base = elppka_base_radix(size);
	if (!base)
		return CRYPTO_INVALID_SIZE;

	ctrl = base << PKA_CTRL_BASE_RADIX;

	/* Handle ECC-521 oddities as a special case. */
	if (size == PKA_ECC521_OPERAND_SIZE) {
		flags |= 1ul << PKA_FLAG_F1;
		ctrl  |= PKA_CTRL_M521_ECC521 << PKA_CTRL_M521_MODE;

		/* Round up partial radix to multiple of ALU size. */
		size = (512 + pka->cfg.alu_size)/8;
	}

	ctrl |= (size & (size-1) ? (size+3)/4 : 0) << PKA_CTRL_PARTIAL_RADIX;
	ctrl |= 1ul << PKA_CTRL_GO;

	vic_pka_io_write32(&pka->regbase[PKA_INDEX_I], 0);
	vic_pka_io_write32(&pka->regbase[PKA_INDEX_J], 0);
	vic_pka_io_write32(&pka->regbase[PKA_INDEX_K], 0);
	vic_pka_io_write32(&pka->regbase[PKA_INDEX_L], 0);

	vic_pka_io_write32(&pka->regbase[PKA_F_STACK], 0);
	vic_pka_io_write32(&pka->regbase[PKA_FLAGS], flags);
	vic_pka_io_write32(&pka->regbase[PKA_ENTRY], entry);
	vic_pka_io_write32(&pka->regbase[PKA_CTRL],  ctrl);

	vic_pka_io_write32(&pka->regbase[PKA_IRQ_EN], 1 << PKA_IRQ_EN_STAT);
   	pka->pka_done = 0;
   	pka->pka_err = 0;

	return 0;
}

void elppka_abort(struct pka_state *pka)
{
	vic_pka_io_write32(&pka->regbase[PKA_CTRL], 1 << PKA_CTRL_STOP_RQST);
}

int elppka_get_status(struct pka_state *pka, unsigned *code)
{
	uint32_t status = vic_pka_io_read32(&pka->regbase[PKA_RC]);

	if (status & (1 << PKA_RC_BUSY)) {
		return CRYPTO_INPROGRESS;
	}

	if (code) {
		*code = (status >> PKA_RC_REASON) & ((1 << PKA_RC_REASON_BITS)-1);
	}

	return 0;
}

int elppka_load_operand(struct pka_state *pka, unsigned bank, unsigned index,
			unsigned size, const uint8_t *data)
{
	uint32_t *opbase, tmp;
	unsigned i, n;
	int rc;

	rc = operand_base_offset(&pka->cfg, bank, index, size);
	if (rc < 0)
		return rc;

	opbase = pka->regbase + rc;
	n = size >> 2;

	for (i = 0; i < n; i++) {
		/*
		 * For lengths that are not a multiple of 4, the incomplete word is
		 * at the _start_ of the data buffer, so we must add the remainder.
		 */
		memcpy(&tmp, data+((n-i-1)<<2)+(size&3), 4);
		vic_pka_io_write32(&opbase[i], tmp);
	}

	/* Write the incomplete word, if any. */
	if (size & 3) {
		tmp = 0;
		memcpy((char *)&tmp + sizeof tmp - (size&3), data, size & 3);
		vic_pka_io_write32(&opbase[i++], tmp);
	}

	/* Zero the remainder of the operand. */
	for (n = elppka_page_size(size) >> 2; i < n; i++) {
		vic_pka_io_write32(&opbase[i], 0);
	}

	return 0;
}

int elppka_unload_operand(struct pka_state *pka, unsigned bank, unsigned index,
			  unsigned size, uint8_t *data)
{
	uint32_t *opbase, tmp;
	unsigned i, n;
	int rc;

	rc = operand_base_offset(&pka->cfg, bank, index, size);
	if (rc < 0)
		return rc;

	opbase = pka->regbase + rc;
	n = size >> 2;

	for (i = 0; i < n; i++) {
		tmp = vic_pka_io_read32(&opbase[i]);
		memcpy(data+((n-i-1)<<2)+(size&3), &tmp, 4);
	}

	if (size & 3) {
		tmp = vic_pka_io_read32(&opbase[i]);
		memcpy(data, (char *)&tmp + sizeof tmp - (size&3), size & 3);
	}

	return 0;
}

void elppka_set_byteswap(struct pka_state *pka, int swap)
{
	uint32_t val = vic_pka_io_read32(&pka->regbase[PKA_CONF]);

	if (swap) {
		val |= 1 << PKA_CONF_BYTESWAP;
	} else {
		val &= ~(1 << PKA_CONF_BYTESWAP);
	}

	vic_pka_io_write32(&pka->regbase[PKA_CONF], val);
}

int elppka_setup(struct pka_state *pka)
{
	const unsigned char big[4]    = { 0x00, 0x11, 0x22, 0x33 };
	const unsigned char little[4] = { 0x33, 0x22, 0x11, 0x00 };
	uint32_t testval = 0x00112233;
	int rc;

	rc = elppka_get_config(pka->regbase, &pka->cfg);
	if (rc < 0)
		return rc;

	/* Try to automatically determine byteswap setting */
	if (!memcmp(&testval, big, sizeof testval)) {
		elppka_set_byteswap(pka, 0);
	} else if (!memcmp(&testval, little, sizeof testval)) {
		elppka_set_byteswap(pka, 1);
	}

	return 0;
}
