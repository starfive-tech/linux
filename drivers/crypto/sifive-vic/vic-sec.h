#ifndef __VIC_SEC_H__
#define __VIC_SEC_H__

#include <crypto/aes.h>
#include <crypto/sha.h>
#include <crypto/engine.h>
#include <linux/delay.h>

#include "vic-pka.h"

#define SEC_IE_REG      0x00
#define SEC_STATUS_REG  0x04

#define MAX_KEY_SIZE		SHA512_BLOCK_SIZE

#define AES_ABLK        1
#define AES_AEAD        2

#define SHA_CLK 1
#define AES_CLK 2
#define PKA_CLK 3

#define AES_CLK_OFFSET 0x0
#define SHA_CLK_OFFSET 0x4
#define PKA_CLK_OFFSET 0x8

#define HASH_OP_UPDATE			1
#define HASH_OP_FINAL			2

#define CFG_REGS_LEN 	32

#define PKA_IO_BASE_OFFSET (32*1024)

union vic_sec_ie {
	u32 v;
	struct {
		u32 sec_done_ie    	:1 ;
		u32 mac_valid_ie	:1 ;
		u32 rsvd_0         	:30;
	};
};

union vic_sec_status {
	u32 v;
	struct {
		u32 sec_done       	:1 ;
		u32 aes_mac_valid  	:1 ;
		u32 aes_busy 	   	:1 ;
		u32 sha_busy       	:1 ;
		u32 rsvd_1         	:28;
	};
};

#define swap32(val) (						\
		     (((u32)(val) << 24) & (u32)0xFF000000) |	\
		     (((u32)(val) <<  8) & (u32)0x00FF0000) |	\
		     (((u32)(val) >>  8) & (u32)0x0000FF00) |	\
		     (((u32)(val) >> 24) & (u32)0x000000FF))

static inline void vic_write_n (void *addr, const u8 *value, unsigned int count)
{
	unsigned int *data = (unsigned int *) value;
	int loop = count >> 2;

	for (; loop--; data++, addr += sizeof(unsigned int))
		writel_relaxed(swap32(*data), addr);

	if(unlikely(count & 0x3)) {
		int ext = count & 0x3;
		value = (u8 *)data;
		for(; ext; ext--){
			writeb_relaxed(*(value + ext - 1), addr + 4 - ext);
		}
	}
}

static inline void vic_read_n (void *addr, unsigned char *out, unsigned int count)
{
	unsigned int *data = (unsigned int *) out;
	int loop = count >> 2;

	for (; loop--; data++, addr += sizeof(unsigned int))
		*data = swap32(readl_relaxed(addr));

	if(unlikely(count & 0x3)) {
		int ext = count & 0x3;
		out = (u8 *)data;
		for(; ext; ext-- )
			*(out + ext - 1) = readb_relaxed(addr + 4 - ext);
	}
}

struct vic_sec_ctx {
	struct crypto_engine_ctx enginectx;
	struct vic_sec_dev      *sdev;
	struct crypto_aead      *sw_cipher;
	unsigned long           flags;

	u8			key[MAX_KEY_SIZE];
	int                     keylen;
	int                     begin_new;
	struct vic_rsa_key      rsa_key;
};

struct vic_sec_dev {
	struct list_head        list;
	struct device           *dev;
	struct clk		*clk;
	void __iomem		*io_base;
	void __iomem		*clk_base;
	void                *data;
	//void                *data_out;
	int                 pages_count;
	struct vic_sec_ctx   *ctx;
	struct vic_sec_request_ctx *rctx;

	struct ahash_request	*req;
	struct crypto_engine	*engine;

	unsigned long           flags;

	union vic_sec_ie        ie;
	union vic_sec_status    status;
	struct mutex 			doing;

	struct mutex            lock; /* protects req / areq */
	struct skcipher_request *sreq;
	struct aead_request     *areq;

	size_t			        data_buf_len;
	size_t			        data_offset;
	size_t                  authsize;

	size_t                  total_in;
	size_t                  total_out;

	bool                    sgs_copied;

	int                     in_sg_len;
	int                     out_sg_len;

	struct scatter_walk     in_walk;
	struct scatter_walk     out_walk;

	u32                     last_ctr[4];
	u32 					ctr_over_count;
	u32                     gcm_ctr;

   struct pka_state pka;
   char fw_name[32];

   /*
    * If you hold a reference to the firmware (obtained by pka_get_firmware),
    * then the fw pointer is guaranteed to remain valid until the reference is
    * dropped; otherwise, one must only access the fw pointer while holding
    * the fw_mutex.
    */
   struct pka_fw_priv *fw;
   struct mutex fw_mutex;

   /*
    * Rather than access PKA flags register directly, store flags to be used
    * for the next operation in work_flags, and cache flags from the previous
    * operation in saved_flags.
    */
   u32 work_flags, saved_flags;
};

// aes

#define VIC_AES_QUEUE_SIZE		512
#define VIC_AES_BUF_ORDER		2

#define VIC_AES_CTRL_REG    0x40
#define VIC_AES_CFG_REGS    0x44

#define VIC_AES_MSG_RAM_OFFSET  (16*1024)
#define VIC_AES_MSG_RAM_SIZE    (8*1024)
// 8160 is mod(32) and mod(24)
#define VIC_AES_BUF_RAM_SIZE    8160

#define VIC_AES_CTX_RAM_OFFSET  (VIC_AES_MSG_RAM_OFFSET + VIC_AES_MSG_RAM_SIZE)
#define VIC_AES_CTX_RAM_SIZE    (4*1024)

#define VIC_AES_CTX_KEYS_OFS    0x00
#define VIC_AES_CTX_KEYS_SIZE   0x20
#define VIC_AES_CTX_CTR_OFS     0x30
#define VIC_AES_CTX_CTR_SIZE    0x20
#define VIC_AES_CTX_IV_OFS      0x40
#define VIC_AES_CTX_IV_SIZE     0x10
#define VIC_AES_CTX_MAC_OFS     0x50
#define VIC_AES_CTX_MAC_SIZE    0x10

#define VIC_AES_IV_LEN          AES_BLOCK_SIZE
#define VIC_AES_CTR_LEN         AES_BLOCK_SIZE

union vic_aes_ctrl {
	unsigned int v;
	struct {
		unsigned int aes_mode        :4 ;
#define VIC_AES_MODE_ECB    0
#define VIC_AES_MODE_CBC    1
#define VIC_AES_MODE_CTR    2
#define VIC_AES_MODE_CCM    3
#define VIC_AES_MODE_CMAC   4
#define VIC_AES_MODE_GCM    5
#define VIC_AES_MODE_OFB    7
#define VIC_AES_MODE_CFB    8
		unsigned int aes_encrypt     :1 ;
#define VIC_AES_DECRYPT     0
#define VIC_AES_ENCRYPT     1
		unsigned int aes_msg_begin   :1 ;
		unsigned int aes_msg_end     :1 ;
		unsigned int aes_str_ctx     :1 ; // Stores intermediate context data back into context memory.
		unsigned int aes_ret_ctx     :1 ; // Retrieves intermediate context data from context memory.
		unsigned int aes_inv_key     :1 ;
		unsigned int aes_str_inv_key :1 ;
		unsigned int rsvd_0          :1 ;
		unsigned int aes_key_sz      :2 ;
#define VIC_AES_KEY_SZ_128  0
#define VIC_AES_KEY_SZ_192  1
#define VIC_AES_KEY_SZ_256  2
		unsigned int rsvd_1          :17; // [30:14]
		unsigned int aes_start       :1 ;
	};
};

union vic_aes_cfg {
	unsigned int vs[1];
	struct {
		// 0x44
		unsigned int aes_tag_msg_addr :13;
		unsigned int rsvd_0           :18; // [30:13]
		unsigned int aes_str_tag2msg  :1 ;

		// 0x48
		unsigned int authsize         :4; //unsigned int aes_mac_len      :4 ;
		unsigned int rsvd_1           :28; // [31:4]

		// 0x4C
		unsigned int aes_blk_idx      :9 ;
#define VIC_AES_BLK_SIZE    0x10
#define VIC_AES_BLKS_NUM    512 //8k/0x10
		unsigned int rsvd_2           :23; // [31:9]

		// 0x50
		unsigned int aes_ctx_idx      :5 ;
#define VIC_AES_CTX_SIZE    0x60
#define VIC_AES_CTXS_NUM    32 //4k/0x60
		unsigned int rsvd_3           :27; // [31:5]

		// 0x54
		unsigned int aes_assoclen     :14; //aes_aad_len      :14;
		unsigned int rsvd_4           :18; // [31:14]

		// 0x58
		unsigned int aes_n_bytes      :14; // Number of bytes of message to cipher in current operation.
		unsigned int rsvd_5           :18; // [31:14]

		// 0x5C
		unsigned int aes_tot_n_bytes  :28; // Total length of message data (across all segments), not including AD, to process. Required in CCM and GCM modes.
		unsigned int rsvd_6           :4 ; // [31:28]

		// 0x60
		unsigned int aes_assoclen_tot :28; //aes_aad_len_tot  :28;
		unsigned int rsvd_7           :4 ; // [31:28]
	};
};

#define KEY_SET_FLAG    1
#define IV_SET_FLAG     (1 << 1)
#define CTR_SET_FLAG    (1 << 2)
#define AD_SET_FLAG     (1 << 3)
#define MAC_SET_FLAG    (1 << 4)


extern void  vic_aes_irq_complete(int irq, void *arg);

// sha
#define HASH_BUFLEN			256

#define HASH_AUTOSUSPEND_DELAY		50
#define CTX_BLOCK_SIZE                  64
#define VIC_MAX_ALIGN_SIZE              128

union vic_sha_ctrl {
	u32 v;
	struct {
		// 0x80
		u32 sha_mode           :4 ;
#define SHA_MODE_224            0
#define SHA_MODE_256            1
#define SHA_MODE_384            2
#define SHA_MODE_512            3
#define SHA_MODE_1              4
#define SHA_MODE_MD5            5
#define SHA_MODE_512_DIV_224    7
#define SHA_MODE_512_DIV_256    8
		u32 sha_hmac           :1 ;
		u32 sha_sslmac         :1 ;
		u32 sha_msg_begin      :1 ;
		u32 sha_msg_end        :1 ;
		u32 sha_store_ctx      :1 ;
		u32 sha_retrieve_ctx   :1 ;
		u32 rsvd_0             :21; // [30:10]
		u32 sha_start          :1 ;
	};
};

union vic_sha_cfg {
	u32 vs[1];
	struct {
		// 0x84
#define SHA_CTX_MSG_ADDR 0x100
#define SHA_CTX_MSG_ADDR_ROUNDS 0x100
		u32 sha_ctx_msg_addr   :13;
		u32 rsvd_0             :18; // [30:13]
		u32 sha_store_ctx_2msg :1 ;

		// 0x88
		u32 sha_secret_bytes   :8 ;
		u32 sha_secret_addr    :11;
		u32 rsvd_1             :13; // [31:19]

		// 0x8C
		u32 sha_num_bytes      :14;
		u32 rsvd_2             :18; // [31:14]

		// 0x90
		u32 sha_icv_len        :6 ;
		u32 rsvd_3             :26; // [31:6]

		// 0x94
		u32 sha_ctx_idx        :6 ;
		u32 rsvd_4             :26; // [31:6]

		// 0x98
		u32 sha_blk_idx        :7 ;
		u32 rsvd_5             :25; // [31:7]

		// 0x9C
		u32 sha_tot_bytes      :26;
		u32 rsvd_6             :6 ; // [31:26]

		// 0xA0
		u32 sha_seqn0             ;
	};
};

struct vic_sec_request_ctx {
	struct vic_sec_dev	*sdev;
	unsigned long		mode;
	unsigned long		flags;
	unsigned long		op;

	u8 digest[CTX_BLOCK_SIZE] __aligned(sizeof(u32));
	size_t			digcnt;
	size_t			bufcnt;
	size_t			buflen;
	size_t			cmac_up_len;

	size_t			assoclen;
	unsigned int            is_load;
	unsigned int             req_type;


	struct scatterlist	*sg;
	struct scatterlist	*out_sg;
	unsigned int		offset;
	unsigned int		total;
	unsigned long			msg_tot;

	union vic_aes_cfg       aes_cfg;
	union vic_aes_ctrl      aes_ctrl;
	union vic_sha_cfg       sha_cfg;
	union vic_sha_ctrl      sha_ctrl;
	unsigned int 	        last_block_idx;

	u8 buffer[HASH_BUFLEN] __aligned(sizeof(u32));
};

static inline void vic_clk_enable(struct vic_sec_dev *sdev, int type)
{
	u32 val;

	switch(type) {
	case AES_CLK:
		val = readl(sdev->clk_base + AES_CLK_OFFSET);
		val &= ~(0x1 << 31);
		val |= 0x1 << 31;
		writel(val, sdev->clk_base + AES_CLK_OFFSET);
		break;
	case PKA_CLK:
		val = readl(sdev->clk_base + PKA_CLK_OFFSET);
		val &= ~(0x1 << 31);
		val |= 0x1 << 31;
		writel(val, sdev->clk_base + PKA_CLK_OFFSET);
		break;
	}
}

static inline void vic_clk_disable(struct vic_sec_dev *sdev, int type)
{
	u32 val;

	switch(type) {
	case AES_CLK:
		val = readl(sdev->clk_base + AES_CLK_OFFSET);
		val &= ~(0x1 << 31);
		val |= 0x0 << 31;
		writel(val, sdev->clk_base + AES_CLK_OFFSET);
		break;
	case PKA_CLK:
		val = readl(sdev->clk_base + PKA_CLK_OFFSET);
		val &= ~(0x1 << 31);
		val |= 0x0 << 31;
		writel(val, sdev->clk_base + PKA_CLK_OFFSET);
		break;
	}
}

#if 0
struct vic_hash_algs_info {
	struct ahash_alg	*algs_list;
	size_t			size;
};

struct vic_hash_pdata {
	struct vic_hash_algs_info	*algs_info;
	size_t				algs_info_size;
};
#endif
extern int vic_aes_register_algs(void);
extern int vic_aes_unregister_algs(void);
extern struct vic_sec_dev *vic_sec_find_dev(struct vic_sec_ctx *ctx);
extern int vic_cryp_get_from_sg(struct vic_sec_request_ctx *rctx, size_t offset,
								size_t count,size_t data_offset);

extern int vic_hash_register_algs(void);
extern int vic_hash_unregister_algs(void);

extern int vic_pka_register_algs(void);
extern int vic_pka_unregister_algs(void);
extern irqreturn_t vic_pka_irq_done(struct vic_sec_dev *sdev);
extern int vic_pka_init(struct pka_state *pka);
#endif
