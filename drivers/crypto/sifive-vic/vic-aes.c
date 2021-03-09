/*
 ******************************************************************************
 * @file  vic-aes.c
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <crypto/hash.h>

#include <crypto/scatterwalk.h>
#include <crypto/internal/aead.h>
#include <crypto/internal/hash.h>
#include <crypto/internal/skcipher.h>

#include "vic-sec.h"


/* Mode mask = bits [3..0] */
#define FLG_MODE_MASK           GENMASK(3, 0)

/* Bit [4] encrypt / decrypt */
#define FLG_ENCRYPT             BIT(4)

/* Bit [31..16] status  */
#define FLG_CCM_PADDED_WA       BIT(5)

#define CR_KEY128               BIT(9)
#define CR_KEY192               BIT(10)
#define CR_KEY256               BIT(11)

/* Registers */
#define CRYP_CR                 0x00000000
#define CRYP_SR                 0x00000004
#define CRYP_DIN                0x00000008
#define CRYP_DOUT               0x0000000C
#define CRYP_DMACR              0x00000010
#define CRYP_IMSCR              0x00000014
#define CRYP_RISR               0x00000018
#define CRYP_MISR               0x0000001C
#define CRYP_K0LR               0x00000020
#define CRYP_K0RR               0x00000024
#define CRYP_K1LR               0x00000028
#define CRYP_K1RR               0x0000002C
#define CRYP_K2LR               0x00000030
#define CRYP_K2RR               0x00000034
#define CRYP_K3LR               0x00000038
#define CRYP_K3RR               0x0000003C
#define CRYP_IV0LR              0x00000040
#define CRYP_IV0RR              0x00000044
#define CRYP_IV1LR              0x00000048
#define CRYP_IV1RR              0x0000004C
#define CRYP_CSGCMCCM0R         0x00000050
#define CRYP_CSGCM0R            0x00000070

#define SR_BUSY                 0x00000010
#define SR_OFNE                 0x00000004

#define IMSCR_IN                BIT(0)
#define IMSCR_OUT               BIT(1)

#define MISR_IN                 BIT(0)
#define MISR_OUT                BIT(1)

/* Misc */
#define AES_BLOCK_32            (AES_BLOCK_SIZE / sizeof(u32))
#define GCM_CTR_INIT            1
#define _walked_in              (cryp->in_walk.offset - cryp->in_sg->offset)
#define _walked_out             (cryp->out_walk.offset - cryp->out_sg->offset)
#define CRYP_AUTOSUSPEND_DELAY	50

static inline int vic_aes_wait_busy(struct vic_sec_dev *sdev)
{
	int ret = -1;

	mutex_lock(&sdev->doing);
	if(sdev->status.sec_done&& (!sdev->status.aes_busy))
		ret = 0;
	mutex_unlock(&sdev->doing);
	return ret;
}

static inline int is_ecb(struct vic_sec_dev *cryp)
{
	return (cryp->flags & FLG_MODE_MASK) == VIC_AES_MODE_ECB;
}

static inline int is_cbc(struct vic_sec_dev *cryp)
{
	return (cryp->flags & FLG_MODE_MASK) == VIC_AES_MODE_CBC;
}

static inline int is_cmac(struct vic_sec_dev *cryp)
{
	return (cryp->flags & FLG_MODE_MASK) == VIC_AES_MODE_CMAC;
}

static inline int is_ofb(struct vic_sec_dev *cryp)
{
	return (cryp->flags & FLG_MODE_MASK) == VIC_AES_MODE_OFB;
}

static inline int is_cfb(struct vic_sec_dev *cryp)
{
	return (cryp->flags & FLG_MODE_MASK) == VIC_AES_MODE_CFB;
}

static inline int is_ctr(struct vic_sec_dev *cryp)
{
	return (cryp->flags & FLG_MODE_MASK) == VIC_AES_MODE_CTR;
}

static inline int is_gcm(struct vic_sec_dev *cryp)
{
	return (cryp->flags & FLG_MODE_MASK) == VIC_AES_MODE_GCM;
}

static inline int is_ccm(struct vic_sec_dev *cryp)
{
	return (cryp->flags & FLG_MODE_MASK) == VIC_AES_MODE_CCM;
}

static inline int get_aes_mode(struct vic_sec_dev *cryp)
{
	return cryp->flags & FLG_MODE_MASK;
}

static inline int is_encrypt(struct vic_sec_dev *cryp)
{
	return !!(cryp->flags & FLG_ENCRYPT);
}

static inline int is_decrypt(struct vic_sec_dev *cryp)
{
	return !is_encrypt(cryp);
}

static int vic_cryp_read_auth_tag(struct vic_sec_dev *sdev);

static void vic_cryp_hw_write_iv(struct vic_sec_dev *sdev, u32 *iv)
{
	if (!iv)
		return;
	if(sdev->ctx->begin_new){
		vic_write_n(sdev->io_base + VIC_AES_CTX_RAM_OFFSET + VIC_AES_CTX_IV_OFS, (u8 *)iv,
		    VIC_AES_IV_LEN);
	}
}

static void vic_cryp_hw_write_ctr(struct vic_sec_dev *sdev, u32 *ctr)
{
	if(sdev->ctx->begin_new){
		vic_write_n(sdev->io_base + VIC_AES_CTX_RAM_OFFSET + VIC_AES_CTX_CTR_OFS, (u8 *)ctr,
			    VIC_AES_IV_LEN);
	}
}

static void vic_cryp_hw_write_key(struct vic_sec_dev *sdev)
{
	if(sdev->ctx->begin_new){
		vic_write_n(sdev->io_base + VIC_AES_CTX_RAM_OFFSET + VIC_AES_CTX_KEYS_OFS, sdev->ctx->key,
			    sdev->ctx->keylen);
	}
}

static unsigned int vic_cryp_get_input_text_len(struct vic_sec_dev *cryp)
{
	return is_encrypt(cryp) ? cryp->areq->cryptlen :
				  cryp->areq->cryptlen - cryp->authsize;
}

static int vic_cryp_gcm_init(struct vic_sec_dev *sdev, u32 cfg)
{
	/* Phase 1 : init */
	memcpy(sdev->last_ctr, sdev->areq->iv, 12);
	sdev->last_ctr[3] = cpu_to_be32(GCM_CTR_INIT);

	vic_cryp_hw_write_ctr(sdev,(u32 *)sdev->last_ctr);
	return 0;
}

static int vic_cryp_write_cryp_out(struct vic_sec_dev *sdev);

static int vic_cryp_ccm_init(struct vic_sec_dev *sdev, u32 cfg)
{
	u8 iv[AES_BLOCK_SIZE], *b0;
	unsigned int textlen;

	/* Phase 1 : init. Firstly set the CTR value to 1 (not 0) */
	memcpy(iv, sdev->areq->iv, AES_BLOCK_SIZE);
	memset(iv + AES_BLOCK_SIZE - 1 - iv[0], 0, iv[0] + 1);
	//iv[AES_BLOCK_SIZE - 1] = 1;

	vic_cryp_hw_write_ctr(sdev,(u32 *)iv);

	/* Build B0 */

	b0 = (u8 *)sdev->data;
	memcpy(b0, iv, AES_BLOCK_SIZE);

	b0[0] |= (8 * ((sdev->authsize - 2) / 2));

	if (sdev->areq->assoclen)
		b0[0] |= 0x40;

	textlen = vic_cryp_get_input_text_len(sdev);

	b0[AES_BLOCK_SIZE - 2] = textlen >> 8;
	b0[AES_BLOCK_SIZE - 1] = textlen & 0xFF;

	memcpy((void *)sdev->last_ctr,sdev->data,AES_BLOCK_SIZE);
	vic_cryp_hw_write_iv(sdev,(u32 *)b0);

	return 0;
}

static int vic_cryp_hw_init(struct vic_sec_dev *cryp)
{
	int ret;
	u32  cfg = cryp->rctx->mode, hw_mode;

	/* Set key */
	vic_cryp_hw_write_key(cryp);

	switch (cryp->ctx->keylen) {
	case AES_KEYSIZE_128:
		cfg |= CR_KEY128;
		break;

	case AES_KEYSIZE_192:
		cfg |= CR_KEY192;
		break;

	default:
	case AES_KEYSIZE_256:
		cfg |= CR_KEY256;
		break;
	}

	hw_mode = get_aes_mode(cryp);

	cfg |= hw_mode;

	memset((void *)cryp->last_ctr, 0, sizeof(cryp->last_ctr));

	switch (hw_mode) {
	case VIC_AES_MODE_GCM:
	case VIC_AES_MODE_CCM:
		/* Phase 1 : init */
		if (hw_mode == VIC_AES_MODE_CCM)
			ret = vic_cryp_ccm_init(cryp, cfg);
		else
			ret = vic_cryp_gcm_init(cryp, cfg);

		if (ret)
			return ret;
		break;

	case VIC_AES_MODE_CBC:
	case VIC_AES_MODE_CFB:
	case VIC_AES_MODE_OFB:
		vic_cryp_hw_write_iv(cryp, (u32 *)cryp->sreq->iv);
		break;
	case VIC_AES_MODE_CTR:
		vic_cryp_hw_write_ctr(cryp, (u32 *)cryp->sreq->iv);
		memcpy((void *)cryp->last_ctr,(void *)cryp->sreq->iv,16);
		break;

	default:
		break;
	}

	cryp->flags |= cfg;

	return 0;
}

int vic_cryp_get_from_sg(struct vic_sec_request_ctx *rctx, size_t offset,
								size_t count,size_t data_offset)
{
	size_t of, ct, index;
	struct scatterlist	*sg = rctx->sg;

	of = offset;
	ct = count;

	while (sg->length <= of){
		of -= sg->length;

		if (!sg_is_last(sg)){
			sg = sg_next(sg);
			continue;
		} else {
			return -EBADE;
		}
	}

	index = data_offset;
	while (ct > 0) {
		if(sg->length - of >= ct) {
			scatterwalk_map_and_copy(rctx->sdev->data + index, sg,
					of, ct, 0);
			index = index + ct;
			return index - data_offset;
		} else {
			scatterwalk_map_and_copy(rctx->sdev->data + index, sg,
					of, sg->length - of, 0);
			index += sg->length - of;
			ct = ct - (sg->length - of);

			of = 0;
		}
		if (!sg_is_last(sg))
			sg = sg_next(sg);
		else
			return -EBADE;
	}
	return index - data_offset;
}

static int vic_cryp_read_auth_tag(struct vic_sec_dev *sdev)
{
	struct vic_sec_request_ctx *rctx = sdev->rctx;
	u32 idigest[SHA512_DIGEST_SIZE / sizeof(u32)];
	int err = 0;

	if(sdev->status.sha_busy || sdev->status.aes_busy) {
		return -EBUSY;
	}

	vic_read_n(sdev->io_base + VIC_AES_CTX_RAM_OFFSET + VIC_AES_CTX_MAC_OFS,
				sdev->rctx->digest,sdev->authsize);


	if(is_cmac(sdev)) {
		if(rctx->op & HASH_OP_FINAL) {
			memcpy(sdev->req->result, sdev->rctx->digest, sdev->authsize);
		} else {
			rctx->is_load = 1;
		}
	} else {
		if(is_encrypt(sdev)) {
			sg_copy_buffer(rctx->out_sg,sg_nents(rctx->out_sg), rctx->digest,
				       sdev->authsize, rctx->offset, 0);
		} else {
			scatterwalk_map_and_copy(idigest,sdev->areq->src, sdev->total_in - sdev->authsize,
						 sdev->authsize, 0);
			if (crypto_memneq(idigest, rctx->digest, sdev->authsize)) {
				err = -EBADMSG;
			}
		}
	}

	return err;
}

static int vic_cryp_read_data(struct vic_sec_dev *sdev)
{
	struct vic_sec_request_ctx *rctx = sdev->rctx;
	int count;

	if(sdev->status.sha_busy || sdev->status.aes_busy) {
		return -EBUSY;
	}
	if(rctx->bufcnt >= sdev->total_out) {
		count = sdev->total_out;
	} else {
		count = rctx->bufcnt;
	}

	vic_read_n(sdev->io_base + VIC_AES_MSG_RAM_OFFSET,sdev->data,
			count);

	sg_copy_buffer(sdev->rctx->out_sg,sg_nents(sdev->rctx->out_sg), sdev->data,
			count, rctx->offset, 0);

	//	sdev->total_out = sdev->total_out - count;

	return 0;
}


static int vic_gcm_zero_message_data(struct vic_sec_dev *sdev);

static int vic_cryp_finish_req(struct vic_sec_dev *sdev, int err)
{
	if (!err && (is_gcm(sdev) || is_ccm(sdev))) {
		/* Phase 4 : output tag */
		err = vic_cryp_read_auth_tag(sdev);
	}

	if (is_gcm(sdev) || is_ccm(sdev)) {
		crypto_finalize_aead_request(sdev->engine, sdev->areq, err);
		sdev->areq = NULL;
	} else {
		crypto_finalize_skcipher_request(sdev->engine, sdev->sreq,
						   err);
		sdev->sreq = NULL;
	}

	memset(sdev->ctx->key, 0, sdev->ctx->keylen);

	return err;
}

static int vic_aes_start(struct vic_sec_request_ctx *rctx)
{
	struct vic_sec_dev *sdev = rctx->sdev;
	int loop, int_len = sizeof(unsigned int);

	if(sdev->status.sha_busy || sdev->status.aes_busy) {
		return -EBUSY;
	}

	for(loop = 0; loop < CFG_REGS_LEN / int_len; loop++) {
		writel(*(rctx->aes_cfg.vs + loop), sdev->io_base + VIC_AES_CFG_REGS + loop * int_len);
	}
	sdev->ie.sec_done_ie = 1;
	sdev->ie.mac_valid_ie = 1;

	mutex_lock(&sdev->doing);
	writel(sdev->ie.v, sdev->io_base + SEC_IE_REG);
	sdev->status.aes_mac_valid = 0;
	sdev->status.aes_busy = 1;
	sdev->status.sec_done = 0;

	rctx->aes_ctrl.aes_start = 1;

	writel(rctx->aes_ctrl.v, sdev->io_base + VIC_AES_CTRL_REG);

	return 0;
}

static int vic_cryp_write_assoc_out(struct vic_sec_dev *sdev)
{
	int ret = 0;
	struct vic_sec_request_ctx *rctx = sdev->rctx;
	unsigned int align_len = rctx->bufcnt % AES_BLOCK_SIZE;
	int ap;

	align_len = AES_BLOCK_SIZE - align_len;

	if(is_ccm(sdev) && align_len) {
		memset(sdev->data + rctx->bufcnt, 0, align_len);
		rctx->bufcnt += align_len;
		sdev->data_offset += align_len;
	}

	memset((void *)&(rctx->aes_cfg), 0, sizeof(rctx->aes_cfg));
	memset((void *)&(rctx->aes_ctrl), 0, sizeof(rctx->aes_ctrl));

	rctx->aes_cfg.authsize = sdev->authsize;
	rctx->aes_cfg.aes_ctx_idx = 0;
	rctx->aes_cfg.aes_blk_idx = 0;

	rctx->aes_ctrl.aes_mode = get_aes_mode(sdev);
	rctx->aes_ctrl.aes_encrypt = is_encrypt(sdev);
	rctx->aes_ctrl.aes_key_sz = (sdev->ctx->keylen >> 3) - 2;
	rctx->aes_ctrl.aes_str_ctx = 1;
	rctx->aes_ctrl.aes_ret_ctx = 1;

	rctx->aes_cfg.aes_assoclen = rctx->bufcnt;
	rctx->aes_cfg.aes_n_bytes = rctx->bufcnt;
	if (rctx->offset == 0)
		rctx->aes_ctrl.aes_msg_begin = 1;
	else
		rctx->aes_ctrl.aes_msg_begin = 0;
	ap = 0;
	if(!is_encrypt(sdev))
		ap = sdev->authsize;

	if ((sdev->total_in - ap - rctx->assoclen == 0) &&
		(rctx->offset + rctx->bufcnt == rctx->assoclen + sdev->data_offset)) {
			rctx->aes_ctrl.aes_msg_end = 1;
			rctx->aes_cfg.aes_assoclen_tot = rctx->assoclen + sdev->data_offset;
	}

	vic_write_n(sdev->io_base + VIC_AES_MSG_RAM_OFFSET, sdev->data,
					rctx->bufcnt);

	ret = vic_aes_start(rctx);
	if (ret)
		return ret;

	if(vic_aes_wait_busy(sdev))
		ret = -ETIMEDOUT;

	return ret;
}

static int vic_cryp_write_cryp_out(struct vic_sec_dev *sdev)
{
	struct vic_sec_request_ctx *rctx = sdev->rctx;
	int ret = 0;
	int ap;

	memset((void *)&(rctx->aes_cfg), 0, sizeof(rctx->aes_cfg));
	memset((void *)&(rctx->aes_ctrl), 0, sizeof(rctx->aes_ctrl));


	rctx->aes_cfg.authsize = sdev->authsize;
	//if(sdev->authsize == 16)
		//	rctx->aes_cfg.authsize = 0xff;
	rctx->aes_cfg.aes_assoclen_tot = rctx->assoclen + sdev->data_offset;
	rctx->aes_ctrl.aes_msg_begin = rctx->assoclen ? 0 : 1;

	rctx->aes_cfg.aes_ctx_idx = 0;
	rctx->aes_cfg.aes_blk_idx = 0;

	rctx->aes_cfg.aes_n_bytes = rctx->bufcnt;

	rctx->aes_ctrl.aes_mode = get_aes_mode(sdev);
	rctx->aes_ctrl.aes_encrypt = is_encrypt(sdev);
	rctx->aes_ctrl.aes_key_sz = (sdev->ctx->keylen >> 3) - 2;

	rctx->aes_ctrl.aes_str_ctx = 1;
	rctx->aes_ctrl.aes_ret_ctx = 1;

	rctx->aes_cfg.aes_tot_n_bytes = sdev->total_in - rctx->assoclen;

	ap = 0;
	if(!is_encrypt(sdev)) {
		rctx->aes_cfg.aes_tot_n_bytes -= sdev->authsize;
		ap = sdev->authsize;
	}

	if (rctx->offset + rctx->bufcnt + ap == sdev->total_in) {
		rctx->aes_ctrl.aes_msg_end = 1;
	} else {
		rctx->aes_ctrl.aes_msg_end = 0;
	}
	if(is_cmac(sdev)) {
		if(rctx->is_load) {
			rctx->aes_ctrl.aes_msg_begin = 0;
			vic_write_n(sdev->io_base + VIC_AES_CTX_RAM_OFFSET + VIC_AES_CTX_MAC_OFS,
					rctx->digest,sdev->authsize);
			rctx->is_load = 0;
		}
	}
	if(rctx->aes_ctrl.aes_msg_begin){
		if(rctx->sdev->ctx->begin_new){
			rctx->sdev->ctx->begin_new = 0;
		}
	}
	vic_write_n(sdev->io_base + VIC_AES_MSG_RAM_OFFSET, sdev->data,
					rctx->bufcnt);
	//	vic_write_n(sdev->io_base + VIC_AES_MSG_RAM_OFFSET, sdev->data,
	//				rctx->bufcnt + AES_BLOCK_SIZE);

	ret = vic_aes_start(rctx);
	if (ret)
		return ret;

	if(vic_aes_wait_busy(sdev))
		ret = -ETIMEDOUT;

	if(!is_cmac(sdev))
		ret = vic_cryp_read_data(sdev);

	return ret;
}

static bool vic_check_counter_overflow(struct vic_sec_dev *sdev, size_t count)
{
	struct vic_sec_request_ctx *rctx = sdev->rctx;
	bool ret = false;
	u32 start, end, ctr, blocks;

	if(count) {
		blocks = DIV_ROUND_UP(count, AES_BLOCK_SIZE);
		sdev->last_ctr[3] = cpu_to_be32(be32_to_cpu(sdev->last_ctr[3]) + blocks);

		if(sdev->last_ctr[3] == 0){
			sdev->last_ctr[2] = cpu_to_be32(be32_to_cpu(sdev->last_ctr[2]) + 1);
			if (sdev->last_ctr[2] == 0){
				sdev->last_ctr[1] = cpu_to_be32(be32_to_cpu(sdev->last_ctr[1]) + 1);
				if (sdev->last_ctr[1] == 0){
					sdev->last_ctr[0] = cpu_to_be32(be32_to_cpu(sdev->last_ctr[0]) + 1);
					if (sdev->last_ctr[1] == 0) {
						vic_cryp_hw_write_ctr(sdev, (u32 *)sdev->last_ctr);
					}
				}
			}
		}
	}

	/* ctr counter overflow. */
	ctr = sdev->total_in - rctx->assoclen - sdev->authsize;
	blocks = DIV_ROUND_UP(ctr, AES_BLOCK_SIZE);
	start = be32_to_cpu(sdev->last_ctr[3]);

	end = start + blocks - 1;
	if (end < start) {
		sdev->ctr_over_count = AES_BLOCK_SIZE * -start;
		ret = true;
	}

	return ret;
}

static int vic_cryp_write_data(struct vic_sec_dev *sdev)
{
	struct vic_sec_request_ctx *rctx = sdev->rctx;
	size_t data_len, total, count, data_buf_len;
	int ret;
	bool fragmented = false;
	u32 data_offset;

	if (unlikely(!sdev->total_in)) {
		dev_warn(sdev->dev, "No more data to process\n");
		return -EINVAL;
	}

	/* ctr counter overflow. */
	fragmented = vic_check_counter_overflow(sdev, 0);

	total = 0;
	rctx->offset = 0;
	sdev->data_offset = 0;

	if(is_ccm(sdev)){
		int index = AES_BLOCK_SIZE;
		if(rctx->assoclen <= 65280) {

			((u8 *)sdev->data)[index] = (rctx->assoclen >> 8) & 0xff;
			((u8 *)sdev->data)[index + 1] = rctx->assoclen  & 0xff;
			sdev->data_offset = index + 2;
		} else {
			((u8 *)sdev->data)[index] = 0xff;
			((u8 *)sdev->data)[index + 1] = 0xfe;
			((u8 *)sdev->data)[index + 2] = rctx->assoclen & 0xFF000000;
			((u8 *)sdev->data)[index + 3] = rctx->assoclen & 0x00FF0000;
			((u8 *)sdev->data)[index + 4] = rctx->assoclen & 0x0000FF00;
			((u8 *)sdev->data)[index + 5] = rctx->assoclen & 0x000000FF;
			sdev->data_offset = index + 6;
		}
	}
	data_offset = sdev->data_offset;
	while(total < rctx->assoclen) {
		data_buf_len = sdev->data_buf_len - (sdev->data_buf_len % sdev->ctx->keylen) - data_offset;
		count = min (rctx->assoclen - rctx->offset, data_buf_len);
		count = min (count, rctx->assoclen - total);
		data_len = vic_cryp_get_from_sg(rctx, rctx->offset, count, data_offset);
		if(data_len < 0)
			return data_len;
		if(data_len != count) {
			return -EINVAL;
		}

		rctx->bufcnt = data_len + data_offset;

		total += data_len;
		ret = vic_cryp_write_assoc_out(sdev);
		if(ret)
			return ret;
		data_offset = 0;
		rctx->offset += data_len;
	}

	total = 0;

	while(total < sdev->total_in - rctx->assoclen) {
		data_buf_len = sdev->data_buf_len - (sdev->data_buf_len % sdev->ctx->keylen) - data_offset;
		count = min (sdev->total_in - rctx->offset, data_buf_len);
		count = min (count, sdev->total_in - rctx->assoclen - total);

		/* ctr counter overflow. */
		if(fragmented && sdev->ctr_over_count != 0) {
			if (count >= sdev->ctr_over_count) {
				count = sdev->ctr_over_count;
			}
		}

		data_len = vic_cryp_get_from_sg(rctx, rctx->offset, count,data_offset);
		if(data_len < 0)
			return data_len;
		if(data_len != count) {
			return -EINVAL;
		}

		rctx->bufcnt = data_len;
		total += data_len;

		if(!is_encrypt(sdev) && (total + rctx->assoclen >= sdev->total_in))
		        rctx->bufcnt = rctx->bufcnt - sdev->authsize;

		if(rctx->bufcnt) {
			ret = vic_cryp_write_cryp_out(sdev);
			if(ret)
				return ret;
		}
		rctx->offset += data_len;

		fragmented = vic_check_counter_overflow(sdev, data_len);
	}

	return 0;
}

static int vic_cmac_write_data(struct vic_sec_dev *sdev)
{
	struct vic_sec_request_ctx *rctx = sdev->rctx;
	size_t data_len, count, data_buf_len;

	int ret;

	if(!sdev->total_in) {
		sdev->rctx->bufcnt = 0;
		sdev->rctx->offset = 0;
		ret = vic_cryp_write_cryp_out(sdev);
		if(ret)
			return ret;
	}

	while(rctx->total < sdev->total_in) {
		data_buf_len = sdev->data_buf_len - (sdev->data_buf_len % sdev->ctx->keylen) - sdev->data_offset;
		count = min (sdev->total_in - rctx->offset, data_buf_len);
		count = min (count, sdev->total_in - rctx->total);

		if(rctx->op == HASH_OP_UPDATE) {
			if((count > sdev->ctx->keylen) && (count % sdev->ctx->keylen)) {
				count = count - count % sdev->ctx->keylen;
			}
		}

		data_len = vic_cryp_get_from_sg(rctx, rctx->offset, count, sdev->data_offset);
		if(data_len < 0)
			return data_len;
		if(data_len != count) {
			return -EINVAL;
		}

		rctx->bufcnt = data_len + sdev->data_offset;
		rctx->total += data_len;

		if((rctx->op == HASH_OP_UPDATE) &&
			(data_len < sdev->ctx->keylen)) {
			sdev->data_offset = data_len;
		} else {
			ret = vic_cryp_write_cryp_out(sdev);
			if(ret)
				return ret;
			rctx->offset += data_len;
		}
	}

	if(sdev->data_offset && (rctx->op & HASH_OP_FINAL)) {
		ret = vic_cryp_write_cryp_out(sdev);
		if(ret)
			return ret;
		rctx->offset += sdev->data_offset;
	}

	return 0;
}

static int vic_gcm_zero_message_data(struct vic_sec_dev *sdev)
{
	int ret;

	sdev->rctx->bufcnt = 0;
	sdev->rctx->offset = 0;
	ret = vic_cryp_write_cryp_out(sdev);

	if(ret)
		return ret;

	return 0;
}

static int vic_cryp_cpu_start(struct vic_sec_dev *sdev, struct vic_sec_request_ctx *rctx)
{
	int ret;

	ret = vic_cryp_write_data(sdev);
	if(ret)
		return ret;

	ret = vic_cryp_finish_req(sdev,ret);

	return ret;
}

static int vic_cryp_cipher_one_req(struct crypto_engine *engine, void *areq);
static int vic_cryp_prepare_cipher_req(struct crypto_engine *engine,
					 void *areq);

static int vic_cryp_cra_init(struct crypto_skcipher *tfm)
{
	struct vic_sec_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->sdev = vic_sec_find_dev(ctx);
	if (!ctx->sdev)
		return -ENODEV;

	mutex_lock(&ctx->sdev->lock);
	vic_clk_enable(ctx->sdev,AES_CLK);

	crypto_skcipher_set_reqsize(tfm, sizeof(struct vic_sec_request_ctx));

	ctx->begin_new = 1;
	ctx->enginectx.op.do_one_request = vic_cryp_cipher_one_req;
	ctx->enginectx.op.prepare_request = vic_cryp_prepare_cipher_req;
	ctx->enginectx.op.unprepare_request = NULL;
	return 0;
}

static void vic_cryp_cra_exit(struct crypto_skcipher *tfm)
{
	struct vic_sec_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->begin_new = 0;
	ctx->enginectx.op.do_one_request = NULL;
	ctx->enginectx.op.prepare_request = NULL;
	ctx->enginectx.op.unprepare_request = NULL;

	vic_clk_disable(ctx->sdev,AES_CLK);
	mutex_unlock(&ctx->sdev->lock);
}

static int vic_cryp_aead_one_req(struct crypto_engine *engine, void *areq);
static int vic_cryp_prepare_aead_req(struct crypto_engine *engine,
				       void *areq);

static int vic_cryp_aes_aead_init(struct crypto_aead *tfm)
{
	struct vic_sec_ctx *ctx = crypto_aead_ctx(tfm);

	ctx->sdev = vic_sec_find_dev(ctx);

	if (!ctx->sdev)
		return -ENODEV;

	mutex_lock(&ctx->sdev->lock);
	vic_clk_enable(ctx->sdev,AES_CLK);

	tfm->reqsize = sizeof(struct vic_sec_request_ctx);

	ctx->begin_new = 1;
	ctx->enginectx.op.do_one_request = vic_cryp_aead_one_req;
	ctx->enginectx.op.prepare_request = vic_cryp_prepare_aead_req;
	ctx->enginectx.op.unprepare_request = NULL;

	return 0;
}

static void vic_cryp_aes_aead_exit(struct crypto_aead *tfm)
{
	struct vic_sec_ctx *ctx = crypto_aead_ctx(tfm);

	ctx->begin_new = 0;
	ctx->enginectx.op.do_one_request = NULL;
	ctx->enginectx.op.prepare_request = NULL;
	ctx->enginectx.op.unprepare_request = NULL;

	vic_clk_disable(ctx->sdev,AES_CLK);
	mutex_unlock(&ctx->sdev->lock);
}

static int vic_cryp_crypt(struct skcipher_request *req, unsigned long mode)
{
	struct vic_sec_ctx *ctx = crypto_skcipher_ctx(
			crypto_skcipher_reqtfm(req));
	struct vic_sec_request_ctx *rctx = skcipher_request_ctx(req);
	struct vic_sec_dev *sdev = ctx->sdev;

	if (!sdev)
		return -ENODEV;

	rctx->mode = mode;
	rctx->req_type = AES_ABLK;

	return crypto_transfer_skcipher_request_to_engine(sdev->engine, req);
}

static int vic_cryp_aead_crypt(struct aead_request *req, unsigned long mode)
{
	struct vic_sec_ctx *ctx = crypto_aead_ctx(crypto_aead_reqtfm(req));
	struct vic_sec_request_ctx *rctx = aead_request_ctx(req);
	struct vic_sec_dev *cryp = ctx->sdev;

	if (!cryp)
		return -ENODEV;

	rctx->mode = mode;
	rctx->req_type = AES_AEAD;

	return crypto_transfer_aead_request_to_engine(cryp->engine, req);
}

static int vic_cryp_setkey(struct crypto_skcipher *tfm, const u8 *key,
			     unsigned int keylen)
{
	struct vic_sec_ctx *ctx = crypto_skcipher_ctx(tfm);

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;

	return 0;
}

static int vic_cryp_aes_setkey(struct crypto_skcipher *tfm, const u8 *key,
				 unsigned int keylen)
{
	if (keylen != AES_KEYSIZE_128 && keylen != AES_KEYSIZE_192 &&
	    keylen != AES_KEYSIZE_256)
		return -EINVAL;
	else
		return vic_cryp_setkey(tfm, key, keylen);
}

static int vic_cryp_aes_aead_setkey(struct crypto_aead *tfm, const u8 *key,
				      unsigned int keylen)
{
	struct vic_sec_ctx *ctx = crypto_aead_ctx(tfm);

	if (keylen != AES_KEYSIZE_128 && keylen != AES_KEYSIZE_192 &&
	    keylen != AES_KEYSIZE_256) {
		return -EINVAL;
	}

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;

	return 0;
}

static int vic_cryp_aes_gcm_setauthsize(struct crypto_aead *tfm,
					  unsigned int authsize)
{
	return authsize == AES_BLOCK_SIZE ? 0 : -EINVAL;
}

static int vic_cryp_aes_ccm_setauthsize(struct crypto_aead *tfm,
					  unsigned int authsize)
{
	switch (authsize) {
	case 4:
	case 6:
	case 8:
	case 10:
	case 12:
	case 14:
	case 16:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int vic_cryp_aes_ecb_encrypt(struct skcipher_request *req)
{
	return vic_cryp_crypt(req,  VIC_AES_MODE_ECB | FLG_ENCRYPT);
}

static int vic_cryp_aes_ecb_decrypt(struct skcipher_request *req)
{
	return vic_cryp_crypt(req,  VIC_AES_MODE_ECB);
}

static int vic_cryp_aes_cbc_encrypt(struct skcipher_request *req)
{
	return vic_cryp_crypt(req,  VIC_AES_MODE_CBC | FLG_ENCRYPT);
}

static int vic_cryp_aes_cbc_decrypt(struct skcipher_request *req)
{
	return vic_cryp_crypt(req,  VIC_AES_MODE_CBC);
}

static int vic_cryp_aes_cfb_encrypt(struct skcipher_request *req)
{
	return vic_cryp_crypt(req,  VIC_AES_MODE_CFB | FLG_ENCRYPT);
}

static int vic_cryp_aes_cfb_decrypt(struct skcipher_request *req)
{
	return vic_cryp_crypt(req,  VIC_AES_MODE_CFB);
}

static int vic_cryp_aes_ofb_encrypt(struct skcipher_request *req)
{
	return vic_cryp_crypt(req,  VIC_AES_MODE_OFB | FLG_ENCRYPT);
}

static int vic_cryp_aes_ofb_decrypt(struct skcipher_request *req)
{
	return vic_cryp_crypt(req,  VIC_AES_MODE_OFB);
}

static int vic_cryp_aes_ctr_encrypt(struct skcipher_request *req)
{
	return vic_cryp_crypt(req,  VIC_AES_MODE_CTR | FLG_ENCRYPT);
}

static int vic_cryp_aes_ctr_decrypt(struct skcipher_request *req)
{
	return vic_cryp_crypt(req,  VIC_AES_MODE_CTR);
}

static int vic_cryp_aes_gcm_encrypt(struct aead_request *req)
{
	return vic_cryp_aead_crypt(req,  VIC_AES_MODE_GCM | FLG_ENCRYPT);
}

static int vic_cryp_aes_gcm_decrypt(struct aead_request *req)
{
	return vic_cryp_aead_crypt(req,  VIC_AES_MODE_GCM);
}

static int vic_cryp_aes_ccm_encrypt(struct aead_request *req)
{
	return vic_cryp_aead_crypt(req,  VIC_AES_MODE_CCM | FLG_ENCRYPT);
}

static int vic_cryp_aes_ccm_decrypt(struct aead_request *req)
{
	return vic_cryp_aead_crypt(req, VIC_AES_MODE_CCM);
}

static int vic_cryp_prepare_req(struct skcipher_request *req,
				  struct aead_request *areq)
{
	struct vic_sec_ctx *ctx;
	struct vic_sec_dev *sdev;
	struct vic_sec_request_ctx *rctx;
	int ret;

	if (!req && !areq)
		return -EINVAL;

	ctx = req ? crypto_skcipher_ctx(crypto_skcipher_reqtfm(req)) :
		    crypto_aead_ctx(crypto_aead_reqtfm(areq));

	sdev = ctx->sdev;

	if (!sdev)
		return -ENODEV;
	if(is_ccm(sdev)){
		if(!areq->assoclen) {
			dev_info(sdev->dev, "AES CCM input assoclen can not be 0\n");
			return -EINVAL;
		}
	}

	rctx = req ? skcipher_request_ctx(req) : aead_request_ctx(areq);

	rctx->sdev = sdev;
	ctx->sdev = sdev;

	sdev->flags = rctx->mode;
	sdev->ctx = ctx;
	sdev->rctx = rctx;

	if (req) {
		sdev->sreq = req;
		sdev->areq = NULL;
		sdev->total_in = req->cryptlen;
		sdev->total_out = sdev->total_in;
		sdev->authsize = 0;
		rctx->assoclen = 0;
	} else {
		/*
		 * Length of input and output data:
		 * Encryption case:
		 *  INPUT  =   AssocData  ||   PlainText
		 *          <- assoclen ->  <- cryptlen ->
		 *          <------- total_in ----------->
		 *
		 *  OUTPUT =   AssocData  ||  CipherText  ||   AuthTag
		 *          <- assoclen ->  <- cryptlen ->  <- authsize ->
		 *          <---------------- total_out ----------------->
		 *
		 * Decryption case:
		 *  INPUT  =   AssocData  ||  CipherText  ||  AuthTag
		 *          <- assoclen ->  <--------- cryptlen --------->
		 *                                          <- authsize ->
		 *          <---------------- total_in ------------------>
		 *
		 *  OUTPUT =   AssocData  ||   PlainText
		 *          <- assoclen ->  <- crypten - authsize ->
		 *          <---------- total_out ----------------->
		 */
		sdev->sreq = NULL;
		sdev->areq = areq;
		sdev->authsize = crypto_aead_authsize(crypto_aead_reqtfm(areq));
		sdev->total_in = areq->assoclen + areq->cryptlen;
		rctx->assoclen = areq->assoclen;
		if (is_encrypt(sdev))
			/* Append auth tag to output */
			sdev->total_out = sdev->total_in + sdev->authsize;
		else
			/* No auth tag in output */
			sdev->total_out = sdev->total_in - sdev->authsize;
	}

	rctx->sg = req ? req->src : areq->src;
	rctx->out_sg = req ? req->dst : areq->dst;

	sdev->in_sg_len = sg_nents_for_len(rctx->sg, sdev->total_in);
	if (sdev->in_sg_len < 0) {
		dev_err(sdev->dev, "Cannot get in_sg_len\n");
		ret = sdev->in_sg_len;
		goto out;
	}

	sdev->out_sg_len = sg_nents_for_len(rctx->out_sg, sdev->total_out);
	if (sdev->out_sg_len < 0) {
		dev_err(sdev->dev, "Cannot get out_sg_len\n");
		ret = sdev->out_sg_len;
		goto out;
	}
#if 0
	if (is_gcm(sdev) || is_ccm(sdev)) {
		/* In output, jump after assoc data */
		sdev->total_out -= sdev->areq->assoclen;
	}
#endif
	ret = vic_cryp_hw_init(sdev);

out:

	return ret;
}

static int vic_cryp_prepare_cipher_req(struct crypto_engine *engine,
					 void *areq)
{
	struct skcipher_request *req = container_of(areq,
						      struct skcipher_request,
						      base);

	return vic_cryp_prepare_req(req, NULL);
}

static int vic_cryp_cipher_one_req(struct crypto_engine *engine, void *areq)
{
	struct skcipher_request *req = container_of(areq,
						      struct skcipher_request,
						      base);
	struct vic_sec_request_ctx *rctx = skcipher_request_ctx(req);
	struct vic_sec_ctx *ctx = crypto_skcipher_ctx(
			crypto_skcipher_reqtfm(req));
	struct vic_sec_dev *cryp = ctx->sdev;

	if (!cryp)
		return -ENODEV;

	return vic_cryp_cpu_start(cryp,rctx);
}

static int vic_cryp_prepare_aead_req(struct crypto_engine *engine, void *areq)
{
	struct aead_request *req = container_of(areq, struct aead_request,
						base);

	return vic_cryp_prepare_req(NULL, req);
}

static int vic_cryp_aead_one_req(struct crypto_engine *engine, void *areq)
{
	struct aead_request *req = container_of(areq, struct aead_request,
						base);
	struct vic_sec_request_ctx *rctx = aead_request_ctx(req);
	struct vic_sec_ctx *ctx = crypto_aead_ctx(crypto_aead_reqtfm(req));
	struct vic_sec_dev *sdev = ctx->sdev;

	if (!sdev)
		return -ENODEV;

	if (unlikely(!sdev->areq->assoclen &&
		     !vic_cryp_get_input_text_len(sdev))) {
		/* No input data to process: get tag and finish */
		vic_gcm_zero_message_data(sdev);
		vic_cryp_finish_req(sdev, 0);
		return 0;
	}

	return vic_cryp_cpu_start(sdev, rctx);
}

static int vic_cryp_prepare_cmac_req(struct crypto_engine *engine,
					 void *areq)
{
	struct ahash_request *req = container_of(areq, struct ahash_request,
						 base);
	struct vic_sec_ctx *ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	struct vic_sec_dev *sdev;
	struct vic_sec_request_ctx *rctx;
	int ret;

	sdev = ctx->sdev;

	if (!sdev)
		return -ENODEV;

	rctx = ahash_request_ctx(req);

	rctx->sdev = sdev;
	ctx->sdev = sdev;

	sdev->flags = rctx->mode;
	sdev->ctx = ctx;
	sdev->rctx = rctx;

	sdev->req = req;
	sdev->total_in = req->nbytes;
	sdev->authsize = AES_BLOCK_SIZE;
	rctx->assoclen = 0;

	//rctx->sg = req->src;

	ret = vic_cryp_hw_init(sdev);

	return ret;
}

static int vic_cryp_cmac_one_req(struct crypto_engine *engine, void *areq)
{
	struct ahash_request *req = container_of(areq, struct ahash_request,
						 base);
	struct vic_sec_ctx *ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	struct vic_sec_dev *sdev = ctx->sdev;
	int ret;

	if (!sdev)
		return -ENODEV;

	ret = vic_cmac_write_data(sdev);

	if(ret)
		return ret;

	ret = vic_cryp_read_auth_tag(sdev);

	crypto_finalize_hash_request(sdev->engine, sdev->req, ret);
	sdev->req = NULL;


	return ret;
}

static int vic_cryp_aes_cmac_init(struct crypto_tfm *tfm)
{
	struct vic_sec_ctx *ctx = crypto_tfm_ctx(tfm);

	ctx->sdev = vic_sec_find_dev(ctx);

	if (!ctx->sdev)
		return -ENODEV;

	mutex_lock(&ctx->sdev->lock);
	vic_clk_enable(ctx->sdev,AES_CLK);

	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct vic_sec_request_ctx));

	ctx->begin_new = 1;
	ctx->enginectx.op.do_one_request = vic_cryp_cmac_one_req;
	ctx->enginectx.op.prepare_request = vic_cryp_prepare_cmac_req;
	ctx->enginectx.op.unprepare_request = NULL;

	return 0;
}

static void vic_cryp_aes_cmac_exit(struct crypto_tfm *tfm)
{
	struct vic_sec_ctx *ctx = crypto_tfm_ctx(tfm);

	ctx->begin_new = 0;
	ctx->enginectx.op.do_one_request = NULL;
	ctx->enginectx.op.prepare_request = NULL;
	ctx->enginectx.op.unprepare_request = NULL;

	vic_clk_disable(ctx->sdev,AES_CLK);
	mutex_unlock(&ctx->sdev->lock);
}

static int vic_aes_cmac_init(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct vic_sec_ctx *ctx = crypto_ahash_ctx(tfm);
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);
	struct vic_sec_dev *sdev = ctx->sdev;

	if (!sdev)
		return -ENODEV;

	rctx->sdev = sdev;
	rctx->mode = VIC_AES_MODE_CMAC | FLG_ENCRYPT;
	rctx->req_type = AES_ABLK;
	rctx->op = 0;
	rctx->is_load = 0;
	rctx->bufcnt = 0;
	rctx->offset = 0;
	sdev->data_offset = 0;

	return 0;
}
static int vic_aes_cmac_update(struct ahash_request *req)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);
	struct vic_sec_dev *sdev = rctx->sdev;

	if (!req->nbytes)
		return 0;

	rctx->total = 0;
	rctx->sg = req->src;
	rctx->offset = 0;

	rctx->op |= HASH_OP_UPDATE;

	return crypto_transfer_hash_request_to_engine(sdev->engine, req);
}

static int vic_aes_cmac_final(struct ahash_request *req)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);
	struct vic_sec_dev *sdev = rctx->sdev;

	rctx->op |= HASH_OP_FINAL;

	return crypto_transfer_hash_request_to_engine(sdev->engine, req);
}

static int vic_aes_cmac_finup(struct ahash_request *req)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);
	int err1, err2;

	rctx->op |= HASH_OP_FINAL | HASH_OP_UPDATE;

	err1 = vic_aes_cmac_update(req);

	if (err1 == -EINPROGRESS || err1 == -EBUSY)
		return err1;

	/*
	 * final() has to be always called to cleanup resources
	 * even if update() failed, except EINPROGRESS
	 */
	err2 = vic_aes_cmac_final(req);

	return err1 ?: err2;
}

static int vic_aes_cmac_digest(struct ahash_request *req)
{
	return vic_aes_cmac_init(req) ?: vic_aes_cmac_finup(req);
}

static int vic_aes_cmac_setkey(struct crypto_ahash *tfm,
			     const u8 *key, unsigned int keylen)
{
	struct vic_sec_ctx *ctx = crypto_ahash_ctx(tfm);


	if (keylen != AES_KEYSIZE_128 && keylen != AES_KEYSIZE_192 &&
	    keylen != AES_KEYSIZE_256)
		return -EINVAL;

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;

	return 0;
}

static int vic_aes_cmac_export(struct ahash_request *req, void *out)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);
	struct vic_sec_dev *sdev = rctx->sdev;

	rctx->digcnt = sdev->authsize;

	vic_read_n(sdev->io_base + VIC_AES_CTX_RAM_OFFSET + VIC_AES_CTX_MAC_OFS,
				sdev->rctx->digest,rctx->digcnt);

	memcpy(out, rctx, sizeof(*rctx));

	return 0;
}

static int vic_aes_cmac_import(struct ahash_request *req, const void *in)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);
	struct vic_sec_dev *sdev = rctx->sdev;

	memcpy(rctx, in, sizeof(*rctx));
	vic_write_n(sdev->io_base + VIC_AES_CTX_RAM_OFFSET + VIC_AES_CTX_MAC_OFS,
				sdev->rctx->digest,rctx->digcnt);
	sdev->authsize = rctx->digcnt;

	return 0;
}

static struct ahash_alg algs_aes_cmac[] = {
	{
		.init = vic_aes_cmac_init,
		.update = vic_aes_cmac_update,
		.final = vic_aes_cmac_final,
		.finup = vic_aes_cmac_finup,
		.digest = vic_aes_cmac_digest,
		.setkey	= vic_aes_cmac_setkey,
		.export = vic_aes_cmac_export,
		.import = vic_aes_cmac_import,
		.halg = {
			.digestsize = AES_BLOCK_SIZE,
			.statesize = sizeof(struct vic_sec_request_ctx),
			.base = {
				.cra_name = "cmac(aes)",
				.cra_driver_name = "vic-cmac-aes",
				.cra_priority = 200,
				.cra_flags = CRYPTO_ALG_ASYNC |
				             CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = AES_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct vic_sec_ctx),
				.cra_alignmask = 3,
				.cra_init = vic_cryp_aes_cmac_init,
				.cra_exit = vic_cryp_aes_cmac_exit,
				.cra_module = THIS_MODULE,
			}
		}
	},
};

static struct skcipher_alg crypto_algs[] = {
{
	.base.cra_name		= "ecb(aes)",
	.base.cra_driver_name	= "vic-ecb-aes",
	.base.cra_priority		= 200,
	.base.cra_flags		= CRYPTO_ALG_ASYNC,
	.base.cra_blocksize		= AES_BLOCK_SIZE,
	.base.cra_ctxsize		= sizeof(struct vic_sec_ctx),
	.base.cra_alignmask		= 0xf,
	.base.cra_module		= THIS_MODULE,
	.init		= vic_cryp_cra_init,
	.exit               = vic_cryp_cra_exit,
	.min_keysize	= AES_MIN_KEY_SIZE,
	.max_keysize	= AES_MAX_KEY_SIZE,
	.setkey		= vic_cryp_aes_setkey,
	.encrypt	= vic_cryp_aes_ecb_encrypt,
	.decrypt	= vic_cryp_aes_ecb_decrypt,
},
{
	.base.cra_name		= "cbc(aes)",
	.base.cra_driver_name	= "vic-cbc-aes",
	.base.cra_priority		= 200,
	.base.cra_flags		=  CRYPTO_ALG_ASYNC,
	.base.cra_blocksize		= AES_BLOCK_SIZE,
	.base.cra_ctxsize		= sizeof(struct vic_sec_ctx),
	.base.cra_alignmask		= 0xf,
	.base.cra_module		= THIS_MODULE,
	.init		= vic_cryp_cra_init,
	.exit               = vic_cryp_cra_exit,
	.min_keysize	= AES_MIN_KEY_SIZE,
	.max_keysize	= AES_MAX_KEY_SIZE,
	.ivsize		= AES_BLOCK_SIZE,
	.setkey		= vic_cryp_aes_setkey,
	.encrypt	= vic_cryp_aes_cbc_encrypt,
	.decrypt	= vic_cryp_aes_cbc_decrypt,
},
{
	.base.cra_name		= "ctr(aes)",
	.base.cra_driver_name	= "vic-ctr-aes",
	.base.cra_priority		= 200,
	.base.cra_flags		= CRYPTO_ALG_ASYNC,
	.base.cra_blocksize		= 1,
	.base.cra_ctxsize		= sizeof(struct vic_sec_ctx),
	.base.cra_alignmask		= 0xf,
	.base.cra_module		= THIS_MODULE,
	.init		= vic_cryp_cra_init,
	.exit               = vic_cryp_cra_exit,
	.min_keysize	= AES_MIN_KEY_SIZE,
	.max_keysize	= AES_MAX_KEY_SIZE,
	.ivsize		= AES_BLOCK_SIZE,
	.setkey		= vic_cryp_aes_setkey,
	.encrypt	= vic_cryp_aes_ctr_encrypt,
	.decrypt	= vic_cryp_aes_ctr_decrypt,
},
{
	.base.cra_name		= "cfb(aes)",
	.base.cra_driver_name	= "vic-cfb-aes",
	.base.cra_priority		= 200,
	.base.cra_flags		= CRYPTO_ALG_ASYNC,
	.base.cra_blocksize		= AES_BLOCK_SIZE,
	.base.cra_ctxsize		= sizeof(struct vic_sec_ctx),
	.base.cra_alignmask		= 0xf,
	.base.cra_module		= THIS_MODULE,
	.init		= vic_cryp_cra_init,
	.exit               = vic_cryp_cra_exit,
	.min_keysize	= AES_MIN_KEY_SIZE,
	.max_keysize	= AES_MAX_KEY_SIZE,
	.ivsize		= AES_BLOCK_SIZE,
	.setkey		= vic_cryp_aes_setkey,
	.encrypt	= vic_cryp_aes_cfb_encrypt,
	.decrypt	= vic_cryp_aes_cfb_decrypt,
},
{
	.base.cra_name		= "ofb(aes)",
	.base.cra_driver_name	= "vic-ofb-aes",
	.base.cra_priority		= 200,
	.base.cra_flags		= CRYPTO_ALG_ASYNC,
	.base.cra_blocksize		= AES_BLOCK_SIZE,
	.base.cra_ctxsize		= sizeof(struct vic_sec_ctx),
	.base.cra_alignmask		= 0xf,
	.base.cra_module		= THIS_MODULE,
	.init		= vic_cryp_cra_init,
	.exit               = vic_cryp_cra_exit,
	.min_keysize	= AES_MIN_KEY_SIZE,
	.max_keysize	= AES_MAX_KEY_SIZE,
	.ivsize		= AES_BLOCK_SIZE,
	.setkey		= vic_cryp_aes_setkey,
	.encrypt	= vic_cryp_aes_ofb_encrypt,
	.decrypt	= vic_cryp_aes_ofb_decrypt,
},
};

static struct aead_alg aead_algs[] = {
#if 1
{
	.setkey		= vic_cryp_aes_aead_setkey,
	.setauthsize	= vic_cryp_aes_gcm_setauthsize,
	.encrypt	= vic_cryp_aes_gcm_encrypt,
	.decrypt	= vic_cryp_aes_gcm_decrypt,
	.init		= vic_cryp_aes_aead_init,
	.exit		= vic_cryp_aes_aead_exit,
	.ivsize		= 12,
	.maxauthsize	= AES_BLOCK_SIZE,

	.base = {
		.cra_name		= "gcm(aes)",
		.cra_driver_name	= "vic-gcm-aes",
		.cra_priority		= 200,
		.cra_flags		= CRYPTO_ALG_ASYNC,
		.cra_blocksize		= 1,
		.cra_ctxsize		= sizeof(struct vic_sec_ctx),
		.cra_alignmask		= 0xf,
		.cra_module		= THIS_MODULE,
	},
},
#endif
{
	.setkey		= vic_cryp_aes_aead_setkey,
	.setauthsize	= vic_cryp_aes_ccm_setauthsize,
	.encrypt	= vic_cryp_aes_ccm_encrypt,
	.decrypt	= vic_cryp_aes_ccm_decrypt,
	.init		= vic_cryp_aes_aead_init,
	.exit		= vic_cryp_aes_aead_exit,
	.ivsize		= AES_BLOCK_SIZE,
	.maxauthsize	= AES_BLOCK_SIZE,

	.base = {
		.cra_name		= "ccm(aes)",
		.cra_driver_name	= "vic-ccm-aes",
		.cra_priority		= 200,
		.cra_flags		= CRYPTO_ALG_ASYNC,
		.cra_blocksize		= 1,
		.cra_ctxsize		= sizeof(struct vic_sec_ctx),
		.cra_alignmask		= 0xf,
		.cra_module		= THIS_MODULE,
	},
},
};
#if 0
int vic_aes_register_algs(void)
{return 0;}

int vic_aes_unregister_algs(void)
{return 0;}
#else
int vic_aes_register_algs(void)
{
	int ret;
#if 1
	ret = crypto_register_ahashes(algs_aes_cmac, ARRAY_SIZE(algs_aes_cmac));
	if (ret) {
		printk("Could not register algs_aes_cmac\n");
		goto err_hash;
	}

	ret = crypto_register_skciphers(crypto_algs, ARRAY_SIZE(crypto_algs));
	if (ret) {
		printk("Could not register algs\n");
		goto err_algs;
	}
#endif
	ret = crypto_register_aeads(aead_algs, ARRAY_SIZE(aead_algs));
	if (ret)
		goto err_aead_algs;

	return 0;

err_aead_algs:
	crypto_unregister_skciphers(crypto_algs, ARRAY_SIZE(crypto_algs));
err_algs:
	crypto_unregister_ahashes(algs_aes_cmac, ARRAY_SIZE(algs_aes_cmac));
err_hash:
	return ret;
}

int vic_aes_unregister_algs(void)
{

	crypto_unregister_aeads(aead_algs, ARRAY_SIZE(aead_algs));
#if 1
	crypto_unregister_skciphers(crypto_algs, ARRAY_SIZE(crypto_algs));

	crypto_unregister_ahashes(algs_aes_cmac, ARRAY_SIZE(algs_aes_cmac));
#endif
	return 0;
}
#endif
