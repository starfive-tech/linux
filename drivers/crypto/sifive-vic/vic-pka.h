#ifndef __VIC_PKA_H__
#define __VIC_PKA_H__
#include <crypto/internal/akcipher.h>
#include <crypto/internal/rsa.h>

#define CRYPTO_OK                      (   0)
#define CRYPTO_FAILED                  (  -1)
#define CRYPTO_INPROGRESS              (  -2)
#define CRYPTO_INVALID_HANDLE          (  -3)
#define CRYPTO_INVALID_CONTEXT         (  -4)
#define CRYPTO_INVALID_SIZE            (  -5)
#define CRYPTO_NOT_INITIALIZED         (  -6)
#define CRYPTO_NO_MEM                  (  -7)
#define CRYPTO_INVALID_ALG             (  -8)
#define CRYPTO_INVALID_KEY_SIZE        (  -9)
#define CRYPTO_INVALID_ARGUMENT        ( -10)
#define CRYPTO_MODULE_DISABLED         ( -11)
#define CRYPTO_NOT_IMPLEMENTED         ( -12)
#define CRYPTO_INVALID_BLOCK_ALIGNMENT ( -13)
#define CRYPTO_INVALID_MODE            ( -14)
#define CRYPTO_INVALID_KEY             ( -15)
#define CRYPTO_AUTHENTICATION_FAILED   ( -16)
#define CRYPTO_INVALID_IV_SIZE         ( -17)
#define CRYPTO_MEMORY_ERROR            ( -18)
#define CRYPTO_LAST_ERROR              ( -19)
#define CRYPTO_HALTED                  ( -20)
#define CRYPTO_TIMEOUT                 ( -21)
#define CRYPTO_SRM_FAILED              ( -22)
#define CRYPTO_COMMON_ERROR_MAX        (-100)
#define CRYPTO_INVALID_ICV_KEY_SIZE    (-100)
#define CRYPTO_INVALID_PARAMETER_SIZE  (-101)
#define CRYPTO_SEQUENCE_OVERFLOW       (-102)
#define CRYPTO_DISABLED                (-103)
#define CRYPTO_INVALID_VERSION         (-104)
#define CRYPTO_FATAL                   (-105)
#define CRYPTO_INVALID_PAD             (-106)
#define CRYPTO_FIFO_FULL               (-107)
#define CRYPTO_INVALID_SEQUENCE        (-108)
#define CRYPTO_INVALID_FIRMWARE        (-109)
#define CRYPTO_NOT_FOUND               (-110)
#define CRYPTO_CMD_FIFO_INACTIVE       (-111)

enum PKA_ENTRY_E {
	PKA_MODMULT           = 10, //0x0a
	PKA_MODADD            = 11, //0x0b
	PKA_MODSUB            = 12, //0x0c
	PKA_MODDIV            = 13, //0x0d
	PKA_MODINV            = 14, //0x0e
	PKA_REDUCE            = 15, //0x0f
	PKA_CALC_MP           = 16, //0x10
	PKA_CALC_R_INV        = 17, //0x11
	PKA_CALC_R_SQR        = 18, //0x12
	PKA_MULT              = 19, //0x13
	PKA_MODEXP            = 20, //0x14
	PKA_CRT_KEY_SETUP     = 21, //0x15
	PKA_CRT               = 22, //0x16
	PKA_BIT_SERIAL_MOD_DP = 23, //0x17
	PKA_BIT_SERIAL_MOD    = 24, //0x18
	PKA_PMULT             = 25, //0x19
	PKA_PDBL              = 26, //0x1a
	PKA_PDBL_STD_PRJ      = 27, //0x1b
	PKA_PADD              = 28, //0x1c
	PKA_PADD_STD_PRJ      = 29, //0x1d
	PKA_PVER              = 30, //0x1e
	PKA_STD_PRJ_TO_AFFINE = 31, //0x1f
	PKA_IS_P_EQUAL_Q      = 32, //0x20
	PKA_IS_P_REFLECT_Q    = 33, //0x21
	PKA_IS_A_M3           = 34, //0x22
	PKA_SHAMIR            = 35, //0x23
	PKA_PMULT_521         = 36, //0x24
	PKA_PDBL_521          = 37, //0x25
	PKA_PADD_521          = 38, //0x26
	PKA_PVER_521          = 39, //0x27
	PKA_M_521_MONTMULT    = 40, //0x28
	PKA_MODMULT_521       = 41, //0x29
	PKA_SHAMIR_521        = 42, //0x2a
};

struct pka_state {
	u32 *regbase;

	struct pka_config {
		unsigned alu_size, rsa_size, ecc_size;
		unsigned fw_ram_size, fw_rom_size;
		unsigned ram_offset, rom_offset;
	} cfg;
	uint32_t pka_done;
	uint32_t pka_err;
};

struct pka_fw {
	unsigned long ram_size, rom_size;
	const char *errmsg;

	struct pka_fw_tag {
		unsigned long origin, tag_length, timestamp, md5_coverage;
		unsigned char md5[16];
	} ram_tag, rom_tag;

	/* For internal use */
	struct elppka_fw_priv *priv;
};

struct vic_rsa_key {
	u8 *n;
	u8 *e;
	u8 *d;
	u8 *p;
	u8 *q;
	u8 *dp;
	u8 *dq;
	u8 *qinv;
	u8 *rinv;
	u8 *rinv_p;
	u8 *rinv_q;
	u8 *mp;
	u8 *rsqr;
	u8 *rsqr_p;
	u8 *rsqr_q;
	u8 *pmp;
	u8 *qmp;
	size_t key_sz;
	bool crt_mode;
};

enum {
	PKA_OPERAND_A,
	PKA_OPERAND_B,
	PKA_OPERAND_C,
	PKA_OPERAND_D,
	PKA_OPERAND_MAX
};

static inline void vic_pka_io_write32(void *addr, unsigned long val)
{
	writel(val,addr);
}

static inline unsigned int vic_pka_io_read32(void *addr)
{
	return readl(addr);
}

int elppka_setup(struct pka_state *pka);

int elppka_start(struct pka_state *pka, uint32_t entry, uint32_t flags,
		 unsigned size);
void elppka_abort(struct pka_state *pka);
int elppka_get_status(struct pka_state *pka, unsigned *code);

int elppka_load_operand(struct pka_state *pka, unsigned bank, unsigned index,
			unsigned size, const uint8_t *data);
int elppka_unload_operand(struct pka_state *pka, unsigned bank, unsigned index,
			  unsigned size, uint8_t *data);

void elppka_set_byteswap(struct pka_state *pka, int swap);

/* Firmware image handling */
int elppka_fw_parse(struct pka_fw *fw, const unsigned char *data,
		    unsigned long len);
void elppka_fw_free(struct pka_fw *fw);

int elppka_fw_lookup_entry(struct pka_fw *fw, const char *entry);

int elppka_fw_load(struct pka_state *pka, struct pka_fw *fw);

/* The firmware timestamp epoch (2009-11-11 11:00:00Z) as a UNIX timestamp. */
#define PKA_FW_TS_EPOCH 1257937200ull

/* Resolution of the timestamp, in seconds. */
#define PKA_FW_TS_RESOLUTION 20

#endif
